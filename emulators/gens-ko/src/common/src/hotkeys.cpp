/* Gens KO transport hotkeys and settings, 2026-11-20.
 * New code is distributed under GNU GPL v2; see LICENSE.txt. */
#include "hotkeys.h"
#include "hotkey_logic.h"
#include "G_Input.h"
#include "G_main.h"
#include "modern_input.h"
#include "resource.h"
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

using namespace GensHotkeys;
static const Binding Defaults[HK_COUNT]={
    {DIK_TAB,0,0,Hold},{DIK_BACK,0,0,Hold},{DIK_SPACE,0,0,Toggle}
};
static Binding Bindings[HK_COUNT]={Defaults[0],Defaults[1],Defaults[2]};
static State States[HK_COUNT];
static PlaybackSettings Settings={100,400,200,50,30,128,true};
static const wchar_t *ActionNames[HK_COUNT]={L"빨리 감기 (FF)",L"되감기 (REW)",L"슬로우모션"};
static const wchar_t *ActionKeys[HK_COUNT]={L"FastForward",L"Rewind",L"SlowMotion"};
static char ConfigPath[1024]="";
static HWND SettingsWindow=NULL;
static bool Capturing=false;

static void WidePath(const char *path,wchar_t *out,unsigned capacity) {
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,out,capacity))
        MultiByteToWideChar(CP_ACP,0,path,-1,out,capacity);
    out[capacity-1]=0;
}
static int ReadInt(const wchar_t *file,const wchar_t *key,int value) {
    return (int)GetPrivateProfileIntW(L"Playback",key,value,file);
}
static bool WriteInt(const wchar_t *file,const wchar_t *key,int value) {
    wchar_t text[32];swprintf(text,32,L"%d",value);
    return WritePrivateProfileStringW(L"Playback",key,text,file)!=0;
}
static unsigned Modifiers(void) {
    return ((GetKeyState(VK_CONTROL)&0x8000)?Control:0)|
        ((GetKeyState(VK_SHIFT)&0x8000)?Shift:0)|((GetKeyState(VK_MENU)&0x8000)?Alt:0)|
        (((GetKeyState(VK_LWIN)|GetKeyState(VK_RWIN))&0x8000)?8:0);
}
static unsigned MessageKey(const MSG &msg) {
    if(msg.wParam==VK_PAUSE) return DIK_PAUSE;
    unsigned key=((unsigned long)msg.lParam>>16)&255;
    if((unsigned long)msg.lParam&0x01000000) key|=0x80;
    return key;
}
static bool KeyboardDown(unsigned key) {
    unsigned scan=(key&0x80)?0xE000|(key&0x7f):key;
    unsigned vk=key==DIK_PAUSE?VK_PAUSE:MapVirtualKeyW(scan,MAPVK_VSC_TO_VK_EX);
    return vk && (GetAsyncKeyState(vk)&0x8000)!=0;
}
void Hotkeys_ClearActive(void) { for(int i=0;i<HK_COUNT;++i) States[i].Clear(); }
bool Hotkeys_IsCapturing(void) { return Capturing; }
bool Hotkeys_IsActive(HotkeyAction action) { return action>=0 && action<HK_COUNT && States[action].active; }
const PlaybackSettings &Hotkeys_Settings(void) { return Settings; }

void Hotkeys_LoadConfig(const char *file) {
    if(!file || !file[0]) return;
    lstrcpynA(ConfigPath,file,sizeof(ConfigPath));wchar_t path[1024];WidePath(file,path,1024);
    for(int i=0;i<HK_COUNT;++i) {
        wchar_t key[80];Binding b=Defaults[i];
        swprintf(key,80,L"%ls.Key",ActionKeys[i]);b.key=(unsigned)ReadInt(path,key,b.key);
        swprintf(key,80,L"%ls.Modifiers",ActionKeys[i]);b.modifiers=(unsigned)ReadInt(path,key,0);
        if(!ValidKeyboard(b.key,b.modifiers)) { b.key=Defaults[i].key;b.modifiers=0; }
        swprintf(key,80,L"%ls.Pad",ActionKeys[i]);b.pad=(unsigned)ReadInt(path,key,0);
        if(!ValidPad(b.pad)) b.pad=0;
        swprintf(key,80,L"%ls.Mode",ActionKeys[i]);int mode=ReadInt(path,key,b.mode);
        b.mode=mode==Toggle?Toggle:Hold;
        /* A damaged config must never fire two transport actions from one key. */
        for(int j=0;j<i;++j) {
            if(SameKey(b,Bindings[j])) b.key=b.modifiers=0;
            if(b.pad && b.pad==Bindings[j].pad) b.pad=0;
        }
        Bindings[i]=b;
    }
    Settings.normalPercent=Clamp(ReadInt(path,L"NormalPercent",100),10,1000);
    Settings.ffPercent=Clamp(ReadInt(path,L"FastForwardPercent",400),100,1600);
    Settings.rewindPercent=Clamp(ReadInt(path,L"RewindPercent",200),25,800);
    Settings.slowPercent=Clamp(ReadInt(path,L"SlowMotionPercent",50),5,100);
    Settings.rewindSeconds=Clamp(ReadInt(path,L"RewindSeconds",30),5,120);
    Settings.rewindMemoryMiB=Clamp(ReadInt(path,L"RewindMemoryMiB",128),32,512);
    Settings.rewindEnabled=ReadInt(path,L"RewindEnabled",1)!=0;
    Hotkeys_ClearActive();
}
static bool SaveSettings(const char *file) {
    if(!file || !file[0]) return false;
    wchar_t path[1024];WidePath(file,path,1024);bool ok=WriteInt(path,L"Version",1);
    for(int i=0;i<HK_COUNT;++i) {
        wchar_t key[80];
        swprintf(key,80,L"%ls.Key",ActionKeys[i]);ok=WriteInt(path,key,Bindings[i].key)&&ok;
        swprintf(key,80,L"%ls.Modifiers",ActionKeys[i]);ok=WriteInt(path,key,Bindings[i].modifiers)&&ok;
        swprintf(key,80,L"%ls.Pad",ActionKeys[i]);ok=WriteInt(path,key,Bindings[i].pad)&&ok;
        swprintf(key,80,L"%ls.Mode",ActionKeys[i]);ok=WriteInt(path,key,Bindings[i].mode)&&ok;
    }
    ok=WriteInt(path,L"NormalPercent",Settings.normalPercent)&&ok;
    ok=WriteInt(path,L"FastForwardPercent",Settings.ffPercent)&&ok;
    ok=WriteInt(path,L"RewindPercent",Settings.rewindPercent)&&ok;
    ok=WriteInt(path,L"SlowMotionPercent",Settings.slowPercent)&&ok;
    ok=WriteInt(path,L"RewindSeconds",Settings.rewindSeconds)&&ok;
    ok=WriteInt(path,L"RewindMemoryMiB",Settings.rewindMemoryMiB)&&ok;
    ok=WriteInt(path,L"RewindEnabled",Settings.rewindEnabled?1:0)&&ok;
    return ok;
}
void Hotkeys_SaveConfig(const char *file) {
    if(file && file[0]) { lstrcpynA(ConfigPath,file,sizeof(ConfigPath));SaveSettings(file); }
}

bool Hotkeys_FilterMessage(MSG *msg) {
    /* Netplay runs a separate emulator loop. Never leave a toggle latched there
     * to activate unexpectedly when the connection closes. */
    if(Net_Play) { Hotkeys_ClearActive();return false; }
    if(!msg || SettingsWindow || ModernInput_IsCapturing() || msg->hwnd!=HWnd || !Input_HasFocus()) return false;
    bool down=msg->message==WM_KEYDOWN || msg->message==WM_SYSKEYDOWN;
    bool up=msg->message==WM_KEYUP || msg->message==WM_SYSKEYUP;
    if(!down && !up) return false;
    unsigned key=MessageKey(*msg),modifiers=Modifiers();bool consumed=false;
    bool repeated=((unsigned long)msg->lParam&0x40000000)!=0;
    for(int i=0;i<HK_COUNT;++i) {
        Binding &b=Bindings[i];State &s=States[i];
        if(up && b.key==key) {
            consumed=consumed||s.keyboard||Match(b,key,modifiers);
            s.SetKeyboard(false,b.mode);
        } else if(down && Match(b,key,modifiers)) {
            s.SetKeyboard(true,b.mode,repeated);consumed=true;
        }
        if(s.keyboard && b.modifiers!=modifiers) s.SetKeyboard(false,b.mode);
    }
    return consumed;
}
void Hotkeys_Poll(void) {
    if(Net_Play || SettingsWindow || ModernInput_IsCapturing() || !Input_HasFocus()) { Hotkeys_ClearActive();return; }
    Update_Input();unsigned modifiers=Modifiers();
    for(int i=0;i<HK_COUNT;++i) {
        Binding &b=Bindings[i];State &s=States[i];
        /* Polling can release a lost key-up but cannot create another toggle. */
        if(s.keyboard && (!KeyboardDown(b.key) || b.modifiers!=modifiers)) s.SetKeyboard(false,b.mode);
        s.SetPad(b.pad && Check_Key_Pressed(b.pad),b.mode);
    }
}

enum {
    HKD_STATUS=3700,HKD_NORMAL,HKD_SECONDS,HKD_MEMORY,HKD_REWIND_ENABLE,HKD_DEFAULT,HKD_APPLY,
    HKD_KEY_BASE=3720,HKD_PAD_BASE=3730,HKD_MODE_BASE=3740,HKD_SPEED_BASE=3750,
    HKD_CLEAR_KEY_BASE=3760,HKD_CLEAR_PAD_BASE=3770
};
struct Editor {
    HWND hwnd,status;HFONT font,titleFont;int dpi,captureAction,captureColumn,releaseTicks;
    DWORD captureStart;bool suppressKeys;Binding bindings[HK_COUNT];
};
static Editor UI;
static int Scale(int x) { return MulDiv(x,UI.dpi,96); }
static HWND Add(const wchar_t *kind,const wchar_t *text,DWORD style,int id,int x,int y,int w,int h,DWORD ex=0) {
    HWND child=CreateWindowExW(ex,kind,text,WS_CHILD|WS_VISIBLE|style,Scale(x),Scale(y),Scale(w),Scale(h),
        UI.hwnd,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(child,WM_SETFONT,(WPARAM)UI.font,TRUE);return child;
}
static void Status(const wchar_t *text) { SetWindowTextW(UI.status,text); }
static void DescribeKey(const Binding &b,wchar_t *text,unsigned capacity) {
    if(!b.key) { lstrcpynW(text,L"미지정",capacity);return; }
    wchar_t name[100];ModernInput_DescribeBinding(b.key,name,100);
    swprintf(text,capacity,L"%ls%ls%ls%ls",(b.modifiers&Control)?L"Ctrl + ":L"",
        (b.modifiers&Shift)?L"Shift + ":L"",(b.modifiers&Alt)?L"Alt + ":L"",name);
}
static bool GameplayConflict(const Binding &binding) {
    return ModernInput_BindingAssigned(binding.key) || ModernInput_BindingAssigned(binding.pad);
}
static bool LegacyShortcutConflict(const Binding &binding) {
    if(!binding.key) return false;
    HACCEL table=LoadAcceleratorsW(GetModuleHandleW(NULL),MAKEINTRESOURCEW(RAC));
    ACCEL entries[128];int count=CopyAcceleratorTableW(table,entries,128);
    unsigned scan=(binding.key&0x80)?0xE000|(binding.key&0x7f):binding.key;
    unsigned vk=binding.key==DIK_PAUSE?VK_PAUSE:MapVirtualKeyW(scan,MAPVK_VSC_TO_VK_EX);
    for(int i=0;i<count;++i) {
        unsigned mods=((entries[i].fVirt&FCONTROL)?Control:0)|((entries[i].fVirt&FSHIFT)?Shift:0)|((entries[i].fVirt&FALT)?Alt:0);
        if((entries[i].fVirt&FVIRTKEY) && entries[i].key==vk && binding.modifiers==mods) return true;
    }
    return false;
}
static void UpdateBindings(void) {
    for(int i=0;i<HK_COUNT;++i) {
        wchar_t label[220];DescribeKey(UI.bindings[i],label,220);
        SetDlgItemTextW(UI.hwnd,HKD_KEY_BASE+i,label);
        ModernInput_DescribeBinding(UI.bindings[i].pad,label,220);
        SetDlgItemTextW(UI.hwnd,HKD_PAD_BASE+i,label);
        SendDlgItemMessageW(UI.hwnd,HKD_MODE_BASE+i,CB_SETCURSEL,UI.bindings[i].mode,0);
    }
}
static void SetNumber(int id,int value) { wchar_t s[32];swprintf(s,32,L"%d",value);SetDlgItemTextW(UI.hwnd,id,s); }
static void SetDefaults(void) {
    for(int i=0;i<HK_COUNT;++i) UI.bindings[i]=Defaults[i];
    UpdateBindings();SetNumber(HKD_NORMAL,100);SetNumber(HKD_SPEED_BASE,400);
    SetNumber(HKD_SPEED_BASE+1,200);SetNumber(HKD_SPEED_BASE+2,50);
    SetNumber(HKD_SECONDS,30);SetNumber(HKD_MEMORY,128);CheckDlgButton(UI.hwnd,HKD_REWIND_ENABLE,BST_CHECKED);
    Status(L"기본값: Tab FF 홀드 · Backspace REW 홀드 · Space 슬로우모션 토글. ‘적용’을 누르면 저장됩니다.");
}
static unsigned FirstPad(void) {
    for(int i=0;i<4;++i) { unsigned code=Input_FirstPressedCode(0x2000|(i<<8));if(code) return code; }
    for(int i=0;i<8;++i) { unsigned code=Input_FirstPressedCode(0x1000|(i<<8));if(code) return code; }
    return 0;
}
static void StopCapture(const wchar_t *text) {
    UI.suppressKeys=UI.suppressKeys||Capturing;Capturing=false;UI.captureAction=-1;
    if(text) Status(text);
}
static void CapturePrompt(void) {
    wchar_t s[300];swprintf(s,300,L"%ls · %ls: %ls. Esc로 취소합니다.",ActionNames[UI.captureAction],
        UI.captureColumn?L"패드 지정":L"키보드 지정",
        UI.releaseTicks<2?L"누른 입력을 모두 떼세요":UI.captureColumn?L"버튼을 누르세요":L"키 또는 Ctrl/Shift/Alt 조합을 누르세요");
    Status(s);
}
static void BeginCapture(int action,int column) {
    UI.captureAction=action;UI.captureColumn=column;UI.releaseTicks=0;UI.captureStart=GetTickCount();
    Capturing=true;UI.suppressKeys=true;SetFocus(UI.hwnd);CapturePrompt();
}
static void FinishCapture(void) {
    int action=UI.captureAction;UpdateBindings();
    bool conflict=false;for(int i=0;i<HK_COUNT;++i) if(i!=action && Conflict(UI.bindings[action],UI.bindings[i])) conflict=true;
    StopCapture(conflict?L"다른 재생 기능과 할당이 겹칩니다. 중복을 해제해야 저장할 수 있습니다.":
        GameplayConflict(UI.bindings[action])?L"게임 버튼과 겹치는 입력입니다. 이 키·버튼을 누르면 게임 입력도 함께 전달됩니다.":
        LegacyShortcutConflict(UI.bindings[action])?L"기존 메뉴 단축키와 겹칩니다. 저장하면 이 키 조합은 지정한 재생 기능에 우선 사용됩니다.":
        L"지정했습니다. ‘적용’ 또는 ‘저장 후 닫기’를 누르면 저장됩니다.");
}
static bool CaptureMessage(const MSG &msg) {
    bool down=msg.message==WM_KEYDOWN || msg.message==WM_SYSKEYDOWN;
    bool up=msg.message==WM_KEYUP || msg.message==WM_SYSKEYUP;
    bool character=msg.message==WM_CHAR || msg.message==WM_SYSCHAR;
    if(!down && !up && !character) return false;
    if(!Capturing) return UI.suppressKeys;
    if(down && msg.wParam==VK_ESCAPE) { StopCapture(L"할당을 취소했습니다.");return true; }
    if(down && UI.captureColumn==0 && UI.releaseTicks>=2 && !(msg.lParam&0x40000000)) {
        unsigned key=MessageKey(msg),modifiers=Modifiers();
        if(ModifierKey(key)) return true;
        if(!ValidKeyboard(key,modifiers)) {
            Status(L"Esc, Windows 키, Alt+Enter, Alt+F4는 사용할 수 없습니다. 다른 키를 누르세요.");return true;
        }
        UI.bindings[UI.captureAction].key=key;UI.bindings[UI.captureAction].modifiers=modifiers;FinishCapture();
    }
    return true;
}
static void CaptureTick(void) {
    Update_Input();
    if(UI.suppressKeys && !Input_FirstPressedCode(0) && !FirstPad()) UI.suppressKeys=false;
    if(!Capturing) return;
    if(!Input_HasFocus()) { UI.releaseTicks=0;return; }
    if((DWORD)(GetTickCount()-UI.captureStart)>20000) { StopCapture(L"입력 대기 시간이 지났습니다. 지정할 항목을 다시 누르세요.");return; }
    unsigned code=UI.captureColumn?FirstPad():Input_FirstPressedCode(0);
    if(UI.releaseTicks<2) {
        if(!code) ++UI.releaseTicks;else UI.releaseTicks=0;
        CapturePrompt();return;
    }
    if(UI.captureColumn && code) { UI.bindings[UI.captureAction].pad=code;FinishCapture(); }
}
static bool ReadNumber(int id,int minimum,int maximum,int &value,const wchar_t *label) {
    BOOL translated=FALSE;unsigned v=GetDlgItemInt(UI.hwnd,id,&translated,FALSE);
    if(!translated || v<(unsigned)minimum || v>(unsigned)maximum) {
        wchar_t text[160];swprintf(text,160,L"%ls 값은 %d~%d 범위로 입력하세요.",label,minimum,maximum);
        Status(text);SetFocus(GetDlgItem(UI.hwnd,id));SendDlgItemMessageW(UI.hwnd,id,EM_SETSEL,0,-1);return false;
    }
    value=(int)v;return true;
}
static bool Commit(void) {
    if(Capturing) { Status(L"할당을 마치거나 Esc로 취소한 뒤 저장하세요.");return false; }
    for(int i=0;i<HK_COUNT;++i) for(int j=0;j<i;++j) if(Conflict(UI.bindings[i],UI.bindings[j])) {
        Status(L"같은 키 조합 또는 패드 입력을 두 기능에 지정할 수 없습니다. 중복 할당을 해제하세요.");return false;
    }
    PlaybackSettings next=Settings;
    if(!ReadNumber(HKD_NORMAL,10,1000,next.normalPercent,L"기본 재생 속도") ||
       !ReadNumber(HKD_SPEED_BASE,100,1600,next.ffPercent,L"FF 속도") ||
       !ReadNumber(HKD_SPEED_BASE+1,25,800,next.rewindPercent,L"REW 속도") ||
       !ReadNumber(HKD_SPEED_BASE+2,5,100,next.slowPercent,L"슬로우모션 속도") ||
       !ReadNumber(HKD_SECONDS,5,120,next.rewindSeconds,L"되감기 보관 시간") ||
       !ReadNumber(HKD_MEMORY,32,512,next.rewindMemoryMiB,L"되감기 메모리")) return false;
    for(int i=0;i<HK_COUNT;++i) {
        UI.bindings[i].mode=SendDlgItemMessageW(UI.hwnd,HKD_MODE_BASE+i,CB_GETCURSEL,0,0)==1?Toggle:Hold;
        Bindings[i]=UI.bindings[i];
    }
    next.rewindEnabled=IsDlgButtonChecked(UI.hwnd,HKD_REWIND_ENABLE)==BST_CHECKED;Settings=next;Hotkeys_ClearActive();
    if(!ConfigPath[0]) {
        wchar_t path[1024];DWORD length=GetModuleFileNameW(NULL,path,1024);path[1023]=0;
        wchar_t *slash=wcsrchr(path,L'\\');
        if(!length || length>=1024 || (slash && slash-path+10>=1024)) {
            Status(L"설정은 적용했지만 실행 경로가 너무 길어 저장하지 못했습니다.");return false;
        }
        if(slash) wcscpy(slash+1,L"Gens.cfg");else wcscpy(path,L"Gens.cfg");
        if(!WideCharToMultiByte(CP_UTF8,0,path,-1,ConfigPath,sizeof(ConfigPath),NULL,NULL)) {
            ConfigPath[0]=0;Status(L"설정은 적용했지만 저장 경로를 변환하지 못했습니다.");return false;
        }
    }
    bool saved=SaveSettings(ConfigPath);
    Status(saved?L"핫키·작동 방식·속도·되감기 설정을 적용하고 저장했습니다.":
        L"설정은 적용했지만 파일 저장에 실패했습니다. 실행 폴더의 쓰기 권한을 확인해 주세요.");
    return saved;
}
static void BuildEditor(HWND hwnd) {
    UI.hwnd=hwnd;SettingsWindow=hwnd;UI.captureAction=-1;
    HDC dc=GetDC(hwnd);UI.dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(hwnd,dc);if(UI.dpi<96) UI.dpi=96;
    MONITORINFO monitor;memset(&monitor,0,sizeof(monitor));monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(GetWindow(hwnd,GW_OWNER),MONITOR_DEFAULTTONEAREST),&monitor);
    int available=monitor.rcWork.bottom-monitor.rcWork.top-60;
    if(Scale(510)>available) UI.dpi=available*96/510;
    int availableWidth=monitor.rcWork.right-monitor.rcWork.left-32;
    if(Scale(850)>availableWidth) UI.dpi=availableWidth*96/850;
    if(UI.dpi<72) UI.dpi=72;
    UI.font=CreateFontW(-MulDiv(10,UI.dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"맑은 고딕");
    UI.titleFont=CreateFontW(-MulDiv(15,UI.dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"맑은 고딕");
    RECT rect={0,0,Scale(850),Scale(510)};DWORD style=WS_POPUP|WS_CAPTION|WS_SYSMENU;
    AdjustWindowRectEx(&rect,style,FALSE,WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT);
    int width=rect.right-rect.left,height=rect.bottom-rect.top;
    SetWindowPos(hwnd,NULL,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,
        monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,SWP_NOZORDER);
    HWND title=Add(L"STATIC",L"재생 속도와 핫키",0,0,20,14,810,30);
    SendMessageW(title,WM_SETFONT,(WPARAM)UI.titleFont,TRUE);
    Add(L"STATIC",L"홀드: 누르는 동안 작동  ·  토글: 한 번 눌러 켜고 다시 눌러 끄기",0,0,20,51,810,24);
    Add(L"STATIC",L"기능",0,0,20,88,140,22);Add(L"STATIC",L"키보드 (눌러서 지정)",0,0,168,88,186,22);
    Add(L"STATIC",L"게임패드 (눌러서 지정)",0,0,403,88,230,22);
    Add(L"STATIC",L"작동 방식",0,0,650,88,90,22);Add(L"STATIC",L"속도 %",0,0,752,88,78,22);
    for(int i=0;i<HK_COUNT;++i) {
        int y=120+i*44;UI.bindings[i]=Bindings[i];
        Add(L"STATIC",ActionNames[i],0,0,20,y+6,142,26);
        Add(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,HKD_KEY_BASE+i,168,y,184,32);
        Add(L"BUTTON",L"해제",WS_TABSTOP|BS_PUSHBUTTON,HKD_CLEAR_KEY_BASE+i,356,y,43,32);
        Add(L"BUTTON",L"",WS_TABSTOP|BS_PUSHBUTTON,HKD_PAD_BASE+i,403,y,190,32);
        Add(L"BUTTON",L"해제",WS_TABSTOP|BS_PUSHBUTTON,HKD_CLEAR_PAD_BASE+i,597,y,43,32);
        HWND mode=Add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,HKD_MODE_BASE+i,650,y+2,92,130);
        SendMessageW(mode,CB_ADDSTRING,0,(LPARAM)L"홀드");SendMessageW(mode,CB_ADDSTRING,0,(LPARAM)L"토글");
        Add(L"EDIT",L"",WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,HKD_SPEED_BASE+i,752,y+2,78,28,WS_EX_CLIENTEDGE);
        SendDlgItemMessageW(hwnd,HKD_SPEED_BASE+i,EM_SETLIMITTEXT,4,0);
    }
    UpdateBindings();SetNumber(HKD_SPEED_BASE,Settings.ffPercent);SetNumber(HKD_SPEED_BASE+1,Settings.rewindPercent);
    SetNumber(HKD_SPEED_BASE+2,Settings.slowPercent);
    Add(L"STATIC",L"기본 재생 속도",0,0,20,257,146,24);
    Add(L"EDIT",L"",WS_TABSTOP|ES_NUMBER,HKD_NORMAL,168,253,80,28,WS_EX_CLIENTEDGE);SetNumber(HKD_NORMAL,Settings.normalPercent);
    Add(L"STATIC",L"%   (100% = 1배속)",0,0,258,257,250,24);
    Add(L"BUTTON",L"되감기 기록 사용",WS_TABSTOP|BS_AUTOCHECKBOX,HKD_REWIND_ENABLE,20,299,200,28);
    CheckDlgButton(hwnd,HKD_REWIND_ENABLE,Settings.rewindEnabled?BST_CHECKED:BST_UNCHECKED);
    Add(L"STATIC",L"보관 시간",0,0,238,303,90,24);
    Add(L"EDIT",L"",WS_TABSTOP|ES_NUMBER,HKD_SECONDS,330,299,65,28,WS_EX_CLIENTEDGE);SetNumber(HKD_SECONDS,Settings.rewindSeconds);
    Add(L"STATIC",L"초",0,0,402,303,30,24);Add(L"STATIC",L"메모리 상한",0,0,467,303,116,24);
    Add(L"EDIT",L"",WS_TABSTOP|ES_NUMBER,HKD_MEMORY,586,299,75,28,WS_EX_CLIENTEDGE);SetNumber(HKD_MEMORY,Settings.rewindMemoryMiB);
    Add(L"STATIC",L"MiB",0,0,670,303,100,24);
    Add(L"STATIC",L"속도는 원래 게임 속도 기준입니다. 우선순위: REW > FF > 슬로우모션.\n1배속이 아닌 재생과 되감기는 무음입니다. 창을 벗어나면 홀드와 토글이 모두 해제됩니다.",0,0,20,340,810,46);
    UI.status=Add(L"STATIC",L"키보드·패드 버튼을 눌러 지정하세요. Esc와 Alt 메뉴 키는 유지됩니다.",0,HKD_STATUS,20,399,810,46);
    Add(L"BUTTON",L"기본값",WS_TABSTOP|BS_PUSHBUTTON,HKD_DEFAULT,20,462,110,32);
    Add(L"BUTTON",L"적용",WS_TABSTOP|BS_PUSHBUTTON,HKD_APPLY,483,462,98,32);
    Add(L"BUTTON",L"저장 후 닫기",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,591,462,137,32);
    Add(L"BUTTON",L"취소",WS_TABSTOP|BS_PUSHBUTTON,IDCANCEL,738,462,92,32);
    Init_Input(GetModuleHandleW(NULL),hwnd);SetTimer(hwnd,1,20,NULL);
}
static LRESULT CALLBACK EditorProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
    switch(message) {
    case WM_CREATE:BuildEditor(hwnd);return 0;
    case WM_TIMER:CaptureTick();return 0;
    case WM_ACTIVATE:
        if(LOWORD(wParam)==WA_INACTIVE) { Input_ClearState();UI.releaseTicks=0; }return 0;
    case WM_COMMAND: {
        int id=LOWORD(wParam);
        if(id==IDCANCEL) { if(Capturing) StopCapture(L"할당을 취소했습니다.");else DestroyWindow(hwnd);return 0; }
        if(id==IDOK) { if(!UI.suppressKeys && Commit()) DestroyWindow(hwnd);return 0; }
        if(id==HKD_APPLY) { Commit();return 0; }
        if(Capturing) return 0;
        if(id==HKD_DEFAULT) { SetDefaults();return 0; }
        if(id>=HKD_KEY_BASE && id<HKD_KEY_BASE+HK_COUNT) { BeginCapture(id-HKD_KEY_BASE,0);return 0; }
        if(id>=HKD_PAD_BASE && id<HKD_PAD_BASE+HK_COUNT) { BeginCapture(id-HKD_PAD_BASE,1);return 0; }
        if(id>=HKD_CLEAR_KEY_BASE && id<HKD_CLEAR_KEY_BASE+HK_COUNT) {
            Binding &b=UI.bindings[id-HKD_CLEAR_KEY_BASE];b.key=b.modifiers=0;UpdateBindings();Status(L"키보드 할당을 해제했습니다.");return 0;
        }
        if(id>=HKD_CLEAR_PAD_BASE && id<HKD_CLEAR_PAD_BASE+HK_COUNT) {
            UI.bindings[id-HKD_CLEAR_PAD_BASE].pad=0;UpdateBindings();Status(L"패드 할당을 해제했습니다.");return 0;
        }
        if(id>=HKD_MODE_BASE && id<HKD_MODE_BASE+HK_COUNT && HIWORD(wParam)==CBN_SELCHANGE) {
            UI.bindings[id-HKD_MODE_BASE].mode=SendDlgItemMessageW(hwnd,id,CB_GETCURSEL,0,0)==1?Toggle:Hold;return 0;
        }
        break;
    }
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:
        KillTimer(hwnd,1);End_Input();Capturing=false;SettingsWindow=NULL;
        if(UI.font) DeleteObject(UI.font);
        if(UI.titleFont) DeleteObject(UI.titleFont);
        return 0;
    }
    return DefWindowProcW(hwnd,message,wParam,lParam);
}
void Hotkeys_ShowDialog(HWND owner) {
    if(SettingsWindow) { SetForegroundWindow(SettingsWindow);return; }
    Hotkeys_ClearActive();HWND inputOwner=Input_GetWindow();
    WNDCLASSW klass;memset(&klass,0,sizeof(klass));klass.lpfnWndProc=EditorProc;klass.hInstance=GetModuleHandleW(NULL);
    klass.hCursor=LoadCursor(NULL,IDC_ARROW);klass.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);klass.lpszClassName=L"GensKOPlaybackSettings";
    RegisterClassW(&klass);memset(&UI,0,sizeof(UI));
    bool wasEnabled=IsWindowEnabled(owner)!=FALSE;if(wasEnabled) EnableWindow(owner,FALSE);
    HWND window=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,klass.lpszClassName,L"핫키 · 재생 속도 설정",
        WS_POPUP|WS_CAPTION|WS_SYSMENU,CW_USEDEFAULT,CW_USEDEFAULT,850,510,owner,NULL,klass.hInstance,NULL);
    bool quit=false;WPARAM exitCode=0;
    if(window) {
        ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(GetDlgItem(window,HKD_KEY_BASE));
        MSG msg;
        while(IsWindow(window)) {
            BOOL got=GetMessageW(&msg,NULL,0,0);if(got<=0) { quit=got==0;exitCode=msg.wParam;break; }
            if(CaptureMessage(msg)) continue;
            if(!IsDialogMessageW(window,&msg)) { TranslateMessage(&msg);DispatchMessageW(&msg); }
        }
        if(IsWindow(window)) DestroyWindow(window);
    }
    if(wasEnabled) EnableWindow(owner,TRUE);
    if(IsWindow(owner)) { SetForegroundWindow(owner);SetFocus(owner); }
    if(inputOwner && IsWindow(inputOwner)) Init_Input(GetModuleHandleW(NULL),inputOwner);
    Hotkeys_ClearActive();if(quit) PostQuitMessage((int)exitCode);
}
