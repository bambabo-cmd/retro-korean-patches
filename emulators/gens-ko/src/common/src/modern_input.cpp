/* Gens KO modern controller configuration.
 * Added 2026-11-20. Distributed under GNU GPL v2; see LICENSE.txt.
 * XInput calls follow the public Microsoft XInputGetState contract; no external
 * implementation or redistributable DLL is copied into this source. */
#include "modern_input.h"
#include "G_Input.h"
#include "G_main.h"
#include "io.h"
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

using namespace GensInput;

/* ABI-compatible declarations avoid a dependency on an old DirectX SDK. */
struct GensXGamepad { WORD buttons; BYTE leftTrigger,rightTrigger; SHORT lx,ly,rx,ry; };
struct GensXState { DWORD packet; GensXGamepad pad; };
typedef DWORD (WINAPI *GensXGetState)(DWORD,GensXState*);
static HMODULE XModule=NULL;
static GensXGetState XGetState=NULL;
static bool XInitialized=false;
static bool XConnected[4]={false};
static DWORD XNextScan[4]={0};
static GensXState XStates[4];
static unsigned PadBinding[8][ActionCount];
static unsigned PadDevice[8]={0x2000,0x2100};
static int Deadzone=25;
static int PreviewDeadzone=-1;
static bool StickDirections=true;
static int PreviewStickDirections=-1;
static bool Loaded=false;
static bool Capturing=false;
static bool SuppressDialogKeys=false;
static char ConfigFile[1024]="";
static HWND SettingsWindow=NULL;
static const char *PlayerKey[8]={"P1","P2","P1B","P1C","P1D","P2B","P2C","P2D"};
static const char *ActionKey[ActionCount]={"Start","Mode","A","B","C","X","Y","Z","Up","Down","Left","Right"};
static const wchar_t *ActionLabel[ActionCount]={L"START",L"MODE",L"A",L"B",L"C",L"X",L"Y",L"Z",L"위",L"아래",L"왼쪽",L"오른쪽"};
static const int RowAction[ActionCount]={Up,Down,Left,Right,A,B,C,X,Y,Z,Start,Mode};

static unsigned &KeyAt(K_Def &keys,int action) {
    switch(action) {
    case Start:return keys.Start;case Mode:return keys.Mode;
    case A:return keys.A;case B:return keys.B;case C:return keys.C;
    case X:return keys.X;case Y:return keys.Y;case Z:return keys.Z;
    case Up:return keys.Up;case Down:return keys.Down;case Left:return keys.Left;
    default:return keys.Right;
    }
}
static unsigned *TypeAt(int player) {
    static unsigned *types[8]={&Controller_1_Type,&Controller_2_Type,&Controller_1B_Type,
        &Controller_1C_Type,&Controller_1D_Type,&Controller_2B_Type,&Controller_2C_Type,&Controller_2D_Type};
    return types[player];
}

static void InitXInput(void) {
    if(XInitialized) return;
    XInitialized=true;
    const wchar_t *names[3]={L"xinput1_4.dll",L"xinput9_1_0.dll",L"xinput1_3.dll"};
    for(int i=0;i<3;++i) {
        wchar_t path[MAX_PATH];UINT n=GetSystemDirectoryW(path,MAX_PATH);
        if(!n || n+24>=MAX_PATH) continue;
        lstrcatW(path,L"\\");lstrcatW(path,names[i]);
        HMODULE module=LoadLibraryW(path);
        if(!module) continue;
        GensXGetState fn=NULL;
        FARPROC address=GetProcAddress(module,"XInputGetState");
        static_assert(sizeof(fn)==sizeof(address),"Win32 function pointer size");
        memcpy(&fn,&address,sizeof(fn));
        if(fn) { XModule=module;XGetState=fn;break; }
        FreeLibrary(module);
    }
}

void ModernInput_ClearState(void) { memset(XStates,0,sizeof(XStates)); }
int ModernInput_Deadzone(void) { return PreviewDeadzone>=0?PreviewDeadzone:Deadzone; }
int ModernInput_IsCapturing(void) { return Capturing; }
int ModernInput_XConnected(int slot) { return slot>=0 && slot<4 && XConnected[slot]; }

void ModernInput_Poll(int foreground) {
    InitXInput();
    DWORD now=GetTickCount();
    for(unsigned i=0;i<4;++i) {
        if(!XGetState) { XConnected[i]=false;memset(&XStates[i],0,sizeof(XStates[i]));continue; }
        if(!XConnected[i] && (LONG)(now-XNextScan[i])<0) continue;
        GensXState state;memset(&state,0,sizeof(state));
        DWORD result=XGetState(i,&state);
        XConnected[i]=result==ERROR_SUCCESS;
        if(XConnected[i] && foreground) XStates[i]=state;
        else memset(&XStates[i],0,sizeof(XStates[i]));
        if(!XConnected[i]) XNextScan[i]=now+2000;
    }
}

int ModernInput_CheckXInput(unsigned code) {
    if(!ValidCode(code) || (code&0xF000)!=0x2000) return 0;
    unsigned slot=(code>>8)&15;
    if(!XConnected[slot]) return 0;
    const GensXGamepad &g=XStates[slot].pad;
    return XPressed(code&255,g.buttons,g.leftTrigger,g.rightTrigger,g.lx,g.ly,g.rx,g.ry,ModernInput_Deadzone());
}
unsigned ModernInput_FirstXInput(int slot) {
    if(slot<0 || slot>=4 || !XConnected[slot]) return 0;
    for(unsigned k=XUp;k<=XRightStickDown;++k) {
        unsigned code=0x2000|(slot<<8)|k;
        if(ModernInput_CheckXInput(code)) return code;
    }
    return 0;
}

static int PadActionPressed(unsigned code) {
    if(Check_Key_Pressed(code)) return 1;
    bool stick=PreviewStickDirections>=0?PreviewStickDirections!=0:StickDirections;
    unsigned alias=StickAlias(code,stick);
    return alias?ModernInput_CheckXInput(alias):0;
}

int ModernInput_ActionPressed(int player,int action) {
    if(player<0 || player>=8 || action<0 || action>=ActionCount) return 0;
    return Check_Key_Pressed(KeyAt(Keys_Def[player],action)) || PadActionPressed(PadBinding[player][action]);
}
bool ModernInput_BindingAssigned(unsigned code) {
    if(!code) return false;
    for(int p=0;p<8;++p) for(int a=0;a<ActionCount;++a)
        if(KeyAt(Keys_Def[p],a)==code || PadBinding[p][a]==code) return true;
    return false;
}

static void InputWide(const char *text,wchar_t *wide,unsigned capacity) {
    if(!text) text="";
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,-1,wide,capacity))
        MultiByteToWideChar(CP_ACP,0,text,-1,wide,capacity);
    wide[capacity-1]=0;
}
static unsigned InputReadInt(const char *section,const char *key,unsigned value,const char *file) {
    wchar_t ws[80],wk[100],wp[1024];InputWide(section,ws,80);InputWide(key,wk,100);InputWide(file,wp,1024);
    return GetPrivateProfileIntW(ws,wk,value,wp);
}
static void InputReadString(const char *section,const char *key,char *out,unsigned capacity,const char *file) {
    wchar_t ws[80],wk[100],wp[1024],value[128];InputWide(section,ws,80);InputWide(key,wk,100);InputWide(file,wp,1024);
    GetPrivateProfileStringW(ws,wk,L"",value,128,wp);
    WideCharToMultiByte(CP_UTF8,0,value,-1,out,capacity,NULL,NULL);out[capacity-1]=0;
}
static bool InputWriteString(const char *section,const char *key,const char *text,const char *file) {
    wchar_t ws[80],wk[100],wp[1024],wt[128];InputWide(section,ws,80);InputWide(key,wk,100);InputWide(file,wp,1024);InputWide(text,wt,128);
    return WritePrivateProfileStringW(ws,wk,wt,wp)!=0;
}
static bool WriteNumber(const char *section,const char *key,unsigned value,const char *file) {
    char text[32];sprintf(text,"%u",value);return InputWriteString(section,key,text,file);
}

void ModernInput_LoadConfig(const char *file) {
    if(!file || !file[0]) return;
    lstrcpynA(ConfigFile,file,sizeof(ConfigFile));
    unsigned version=InputReadInt("Input.Modern","Version",0,file);
    if(version<2) {
        unsigned old[ActionCount];for(int a=0;a<ActionCount;++a) old[a]=KeyAt(Keys_Def[0],a);
        /* Only replace an entire unchanged r1 layout; keep every custom layout. */
        if(IsLegacyP1Default(old)) for(int a=0;a<ActionCount;++a) KeyAt(Keys_Def[0],a)=KeyboardDefaultCode(0,a);
    }
    Deadzone=ClampDeadzone(InputReadInt("Input.Modern","DeadzonePercent",25,file));
    StickDirections=InputReadInt("Input.Modern","LeftStickDirections",1,file)!=0;
    for(int p=0;p<8;++p) {
        unsigned inferred=0;
        for(int a=0;a<ActionCount;++a) {
            char key[64];sprintf(key,"%s.Pad.%s",PlayerKey[p],ActionKey[a]);
            unsigned legacy=KeyAt(Keys_Def[p],a);
            unsigned def=(p<2)?XDefault(a,p):0;
            if(legacy>=256 && ValidCode(legacy)) {
                def=legacy;KeyAt(Keys_Def[p],a)=0;
            }
            if(!ValidCode(KeyAt(Keys_Def[p],a))) KeyAt(Keys_Def[p],a)=0;
            unsigned value=version?InputReadInt("Input.Modern",key,def,file):def;
            if(!ValidCode(value) || value<256) value=0;
            PadBinding[p][a]=value;
            if(value && !inferred) inferred=value&0xFF00;
        }
        char key[64];sprintf(key,"%s.Device",PlayerKey[p]);
        unsigned device=InputReadInt("Input.Modern",key,inferred,file);
        if(device && !ValidCode(device|1)) device=inferred;
        PadDevice[p]=device;
    }
    HWND inputOwner=Input_GetWindow();
    if(inputOwner) End_Input();
    Input_ResetJoystickPreferences();
    for(int i=0;i<8;++i) {
        char key[32],guid[64];sprintf(key,"DirectInput.Guid%d",i);
        InputReadString("Input.Modern",key,guid,sizeof(guid),file);
        Input_SetJoystickGuid(i,guid);
    }
    if(inputOwner) Init_Input(GetModuleHandleW(NULL),inputOwner);
    Loaded=true;
}

static bool SaveInput(const char *file) {
    if(!file || !file[0]) return false;
    bool ok=true;
    ok=WriteNumber("Input.Modern","Version",2,file)&&ok;
    ok=WriteNumber("Input.Modern","DeadzonePercent",Deadzone,file)&&ok;
    ok=WriteNumber("Input.Modern","LeftStickDirections",StickDirections?1:0,file)&&ok;
    for(int p=0;p<8;++p) {
        char key[64];sprintf(key,"%s.Type",PlayerKey[p]);
        ok=WriteNumber("Input",key,*TypeAt(p)&(p<2?0x13:0x03),file)&&ok;
        for(int a=0;a<ActionCount;++a) {
            sprintf(key,"%s.%s",PlayerKey[p],ActionKey[a]);
            unsigned primary=KeyAt(Keys_Def[p],a);
            if(!primary && (PadBinding[p][a]&0xF000)==0x1000) primary=PadBinding[p][a];
            ok=WriteNumber("Input",key,primary,file)&&ok;
            sprintf(key,"%s.Pad.%s",PlayerKey[p],ActionKey[a]);
            ok=WriteNumber("Input.Modern",key,PadBinding[p][a],file)&&ok;
        }
        sprintf(key,"%s.Device",PlayerKey[p]);ok=WriteNumber("Input.Modern",key,PadDevice[p],file)&&ok;
    }
    for(int i=0;i<8;++i) {
        char key[32],guid[64]={0};sprintf(key,"DirectInput.Guid%d",i);
        Input_JoystickGuid(i,guid,sizeof(guid));
        if(guid[0]) ok=InputWriteString("Input.Modern",key,guid,file)&&ok;
    }
    return ok;
}

void ModernInput_SaveConfig(const char *file) {
    if(file && file[0]) { lstrcpynA(ConfigFile,file,sizeof(ConfigFile));SaveInput(file); }
}

void ModernInput_DescribeBinding(unsigned code,wchar_t *out,unsigned capacity) {
    if(!out || !capacity) return;
    if(!code || !ValidCode(code)) { lstrcpynW(out,L"미지정",capacity);return; }
    if(code<256) {
        LONG flags=((code&0x7F)<<16)|((code&0x80)?0x01000000:0);
        if(GetKeyNameTextW(flags,out,capacity)) return;
        swprintf(out,capacity,L"키 0x%02X",code);return;
    }
    unsigned slot=(code>>8)&15,value=code&255;
    if((code&0xF000)==0x2000) {
        static const wchar_t *names[]={L"",L"방향키 ↑",L"방향키 ↓",L"방향키 ←",L"방향키 →",
            L"START",L"BACK",L"왼쪽 스틱 클릭",L"오른쪽 스틱 클릭",L"LB",L"RB",L"A",L"B",L"X",L"Y",L"LT",L"RT",
            L"왼쪽 스틱 ←",L"왼쪽 스틱 →",L"왼쪽 스틱 ↑",L"왼쪽 스틱 ↓",L"오른쪽 스틱 ←",L"오른쪽 스틱 →",L"오른쪽 스틱 ↑",L"오른쪽 스틱 ↓"};
        const wchar_t *name=names[value];
        bool stick=PreviewStickDirections>=0?PreviewStickDirections!=0:StickDirections;
        if(stick && value>=1 && value<=4) {
            static const wchar_t *both[]={L"",L"방향키/스틱 ↑",L"방향키/스틱 ↓",L"방향키/스틱 ←",L"방향키/스틱 →"};
            name=both[value];
        }
        swprintf(out,capacity,L"XInput %u · %ls",slot+1,name);return;
    }
    if(value>=0x10 && value<0x30) swprintf(out,capacity,L"패드 %u · 버튼 %u",slot+1,value-0x10+1);
    else if((value&0xC0)==0x80) {
        const wchar_t *arrows[]={L"",L"↑",L"→",L"↓",L"←"};
        swprintf(out,capacity,L"패드 %u · 방향키 %u %ls",slot+1,((value>>4)&3)+1,arrows[value&15]);
    } else if(value>=0x40) {
        const wchar_t *axis[]={L"X",L"Y",L"Z",L"RX",L"RY",L"RZ",L"슬라이더 1",L"슬라이더 2"};
        swprintf(out,capacity,L"패드 %u · %ls %ls",slot+1,axis[(value-0x40)/2],(value&1)?L"+":L"−");
    } else {
        const wchar_t *arrows[]={L"",L"↑",L"↓",L"←",L"→"};
        swprintf(out,capacity,L"패드 %u · 스틱 %ls",slot+1,arrows[value]);
    }
}

/* Native Unicode editor: keyboard and pad bindings stay independent. */
enum {
    ID_PLAYER=3100,ID_DEVICE,ID_TYPE,ID_LIST,ID_KEY_CAPTURE,ID_PAD_CAPTURE,ID_CLEAR,
    ID_SEQUENCE,ID_KEY_DEFAULT,ID_PAD_DEFAULT,ID_SCAN,ID_DEADZONE,ID_DEAD_LABEL,
    ID_STATUS,ID_APPLY,ID_PORT1,ID_PORT2,ID_HEADING,ID_HINT,ID_STICK
};
struct InputEditor {
    HWND hwnd,list,status;
    HFONT font,titleFont;
    unsigned keyboard[8][ActionCount],pad[8][ActionCount],device[8],types[8];
    int player,row,column,deadzone,dpi;
    int captureRow,captureColumn,releaseTicks;
    bool sequence,stickDirections;
    DWORD captureStart,nextNames;
    int lastPressed[ActionCount];
    unsigned deviceFingerprint,lastDevice;
    int devicePlayer;
    wchar_t lastStatus[400];
};
static InputEditor UI;
static int Scale(int value) { return MulDiv(value,UI.dpi,96); }

static HWND AddControl(const wchar_t *kind,const wchar_t *title,DWORD style,int id,int x,int y,int w,int h,DWORD extended=0) {
    HWND child=CreateWindowExW(extended,kind,title,WS_CHILD|WS_VISIBLE|style,Scale(x),Scale(y),Scale(w),Scale(h),
        UI.hwnd,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(child,WM_SETFONT,(WPARAM)UI.font,TRUE);return child;
}
static void Status(const wchar_t *text) {
    if(wcscmp(UI.lastStatus,text)) { lstrcpynW(UI.lastStatus,text,400);SetWindowTextW(UI.status,text); }
}
static bool IsSixButton(int player) { return (UI.types[player]&1)!=0; }
static bool ActionAvailable(int action) { return IsSixButton(UI.player) || (action!=Mode && action!=X && action!=Y && action!=Z); }
static void SetCell(int row,int column,const wchar_t *text) {
    LVITEMW item;memset(&item,0,sizeof(item));item.iSubItem=column;item.pszText=(LPWSTR)text;
    SendMessageW(UI.list,LVM_SETITEMTEXTW,row,(LPARAM)&item);
}
static void SelectRow(int row) {
    UI.row=row;
    ListView_SetItemState(UI.list,row,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    ListView_EnsureVisible(UI.list,row,FALSE);
}
static bool IsDuplicate(int action,int column) {
    const unsigned *bindings=column==1?UI.keyboard[UI.player]:UI.pad[UI.player];
    if(!bindings[action]) return false;
    for(int a=0;a<ActionCount;++a) if(a!=action && bindings[a]==bindings[action]) return true;
    return false;
}
static void UpdateRows(void) {
    for(int r=0;r<ActionCount;++r) {
        int action=RowAction[r];wchar_t label[220];
        SetCell(r,0,ActionLabel[action]);
        ModernInput_DescribeBinding(UI.keyboard[UI.player][action],label,220);SetCell(r,1,label);
        ModernInput_DescribeBinding(UI.pad[UI.player][action],label,220);SetCell(r,2,label);
        const wchar_t *state=L"";
        if(!ActionAvailable(action)) state=L"6버튼 전용";
        else if(IsDuplicate(action,1)||IsDuplicate(action,2)) state=L"중복 할당";
        SetCell(r,3,state);UI.lastPressed[r]=-1;
    }
    InvalidateRect(UI.list,NULL,FALSE);
}
static void UpdateDeviceList(void) {
    HWND combo=GetDlgItem(UI.hwnd,ID_DEVICE);
    unsigned fingerprint=0;
    for(int i=0;i<4;++i) if(ModernInput_XConnected(i)) fingerprint|=1u<<i;
    for(int i=0;i<8;++i) {
        if(Input_JoystickKnown(i)) fingerprint|=1u<<(i+4);
        if(Input_JoystickConnected(i)) fingerprint|=1u<<(i+12);
    }
    if(UI.devicePlayer==UI.player && UI.deviceFingerprint==fingerprint && UI.lastDevice==UI.device[UI.player]) return;
    if(SendMessageW(combo,CB_GETDROPPEDSTATE,0,0)) return;
    UI.devicePlayer=UI.player;UI.deviceFingerprint=fingerprint;UI.lastDevice=UI.device[UI.player];
    SendMessageW(combo,CB_RESETCONTENT,0,0);
    int index=(int)SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)L"패드 사용 안 함");
    SendMessageW(combo,CB_SETITEMDATA,index,0);int selected=0;
    for(int i=0;i<4;++i) {
        wchar_t name[128];swprintf(name,128,L"XInput %d  —  %ls",i+1,ModernInput_XConnected(i)?L"연결됨":L"연결 대기");
        index=(int)SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)name);
        unsigned code=0x2000|(i<<8);SendMessageW(combo,CB_SETITEMDATA,index,code);
        if(UI.device[UI.player]==code) selected=index;
    }
    for(int i=0;i<8;++i) if(Input_JoystickKnown(i)) {
        wchar_t device[200],name[270];Input_JoystickName(i,device,200);
        swprintf(name,270,L"DirectInput %d · %ls%ls",i+1,device,Input_JoystickConnected(i)?L"":L" (연결 대기)");
        index=(int)SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)name);
        unsigned code=0x1000|(i<<8);SendMessageW(combo,CB_SETITEMDATA,index,code);
        if(UI.device[UI.player]==code) selected=index;
    }
    SendMessageW(combo,CB_SETCURSEL,selected,0);
}
static void UpdatePlayer(void) {
    UpdateDeviceList();
    SendDlgItemMessageW(UI.hwnd,ID_TYPE,CB_SETCURSEL,IsSixButton(UI.player)?1:0,0);
    UpdateRows();SelectRow(UI.row<0?0:UI.row);
    if(UI.player>=2) Status(L"멀티탭 플레이어입니다. 해당 포트의 멀티탭을 켜야 게임에서 사용할 수 있습니다.");
    else Status(L"키보드와 게임패드는 동시에 사용할 수 있습니다. 셀을 두 번 클릭하거나 아래 지정 버튼을 누르세요.");
}
static void StopCapture(const wchar_t *message) {
    SuppressDialogKeys=Capturing || SuppressDialogKeys;
    Capturing=false;UI.captureRow=-1;UI.sequence=false;UI.releaseTicks=0;
    EnableWindow(GetDlgItem(UI.hwnd,ID_PLAYER),TRUE);
    EnableWindow(GetDlgItem(UI.hwnd,ID_DEVICE),TRUE);
    EnableWindow(GetDlgItem(UI.hwnd,ID_TYPE),TRUE);
    if(message) Status(message);
}
static void CapturePrompt(void) {
    wchar_t text[400];int action=RowAction[UI.captureRow];
    swprintf(text,400,L"%ls → %ls 지정: %ls. Esc로 취소%ls",ActionLabel[action],UI.captureColumn==1?L"키보드":L"게임패드",
        UI.releaseTicks<2?L"누른 키와 버튼을 모두 떼세요":L"할당할 키 또는 버튼을 누르세요",UI.sequence?L" · 순서대로 지정 중":L"");
    Status(text);
}
static void BeginCapture(int column,bool sequence) {
    if(UI.row<0) SelectRow(0);
    if(!ActionAvailable(RowAction[UI.row])) { Status(L"이 버튼은 6버튼 패드에서 사용합니다. 위에서 6버튼을 선택하세요.");return; }
    if(column==2 && !UI.device[UI.player]) { Status(L"먼저 게임패드 장치를 선택하세요.");return; }
    UI.column=column;UI.captureRow=UI.row;UI.captureColumn=column;UI.sequence=sequence;
    UI.releaseTicks=0;UI.captureStart=GetTickCount();Capturing=true;
    EnableWindow(GetDlgItem(UI.hwnd,ID_PLAYER),FALSE);
    EnableWindow(GetDlgItem(UI.hwnd,ID_DEVICE),FALSE);
    EnableWindow(GetDlgItem(UI.hwnd,ID_TYPE),FALSE);
    SetFocus(UI.list);CapturePrompt();
}
static void CaptureTick(void) {
    if(!Capturing) return;
    if(!Input_HasFocus()) { UI.releaseTicks=0;return; }
    if(Check_Key_Pressed(DIK_ESCAPE)) { StopCapture(L"할당을 취소했습니다. 이미 지정한 항목은 유지됩니다.");return; }
    unsigned code=Input_FirstPressedCode(UI.captureColumn==1?0:(int)UI.device[UI.player]);
    if(UI.releaseTicks<2) {
        if(!code) ++UI.releaseTicks;else UI.releaseTicks=0;
        CapturePrompt();return;
    }
    if((DWORD)(GetTickCount()-UI.captureStart)>15000) { StopCapture(L"입력 대기 시간이 지났습니다. 지정 버튼을 다시 눌러 주세요.");return; }
    if(!code) return;
    int action=RowAction[UI.captureRow];
    if(UI.captureColumn==1) UI.keyboard[UI.player][action]=code;
    else UI.pad[UI.player][action]=code;
    UpdateRows();
    if(UI.sequence) {
        int next=UI.captureRow+1;
        while(next<ActionCount && !ActionAvailable(RowAction[next])) ++next;
        if(next<ActionCount) { SelectRow(next);UI.captureRow=next;UI.releaseTicks=0;UI.captureStart=GetTickCount();CapturePrompt();return; }
    }
    StopCapture((IsDuplicate(action,UI.captureColumn))?L"할당했습니다. 같은 입력이 다른 버튼에도 지정되어 있습니다.":L"할당했습니다. 실제 입력은 오른쪽 ‘입력 확인’ 열에서 확인할 수 있습니다.");
}
static void LiveTick(void) {
    for(int r=0;r<ActionCount;++r) {
        int a=RowAction[r];
        int pressed=ActionAvailable(a) && (Check_Key_Pressed(UI.keyboard[UI.player][a]) || PadActionPressed(UI.pad[UI.player][a]));
        if(pressed==UI.lastPressed[r]) continue;
        UI.lastPressed[r]=pressed;
        const wchar_t *state=pressed?L"● 눌림":!ActionAvailable(a)?L"6버튼 전용":
            (IsDuplicate(a,1)||IsDuplicate(a,2))?L"중복 할당":L"—";
        SetCell(r,3,state);
    }
}
static void ClearSelected(void) {
    if(UI.row<0) return;
    StopCapture(NULL);
    int action=RowAction[UI.row];
    if(UI.column==2) UI.pad[UI.player][action]=0;else UI.keyboard[UI.player][action]=0;
    UpdateRows();Status(UI.column==2?L"선택한 게임패드 할당을 해제했습니다.":L"선택한 키보드 할당을 해제했습니다.");
}
static void DefaultKeyboard(void) {
    StopCapture(NULL);
    for(int a=0;a<ActionCount;++a) UI.keyboard[UI.player][a]=KeyboardDefaultCode(UI.player,a);
    UI.types[UI.player]|=1;SendDlgItemMessageW(UI.hwnd,ID_TYPE,CB_SETCURSEL,1,0);
    UpdateRows();Status(L"선택한 플레이어를 6버튼 패드로 설정하고 키보드 기본값을 복원했습니다.");
}
static void DefaultPad(void) {
    StopCapture(NULL);unsigned device=UI.device[UI.player];
    if(!device) { Status(L"먼저 게임패드 장치를 선택하세요.");return; }
    UI.types[UI.player]|=1;SendDlgItemMessageW(UI.hwnd,ID_TYPE,CB_SETCURSEL,1,0);
    if((device&0xF000)==0x2000) for(int a=0;a<ActionCount;++a) UI.pad[UI.player][a]=XDefault(a,(device>>8)&15);
    else {
        static const unsigned values[ActionCount]={0x17,0x16,0x10,0x11,0x12,0x13,0x14,0x15,0x81,0x83,0x84,0x82};
        for(int a=0;a<ActionCount;++a) UI.pad[UI.player][a]=device|values[a];
    }
    UpdateRows();Status((device&0xF000)==0x2000?L"XInput 기본값: A/B/C = X/A/B, X/Y/Z = LB/Y/RB, START/BACK = 시작/모드.":
        L"DirectInput 기본값을 넣었습니다. 패드마다 버튼 번호가 다르므로 입력 확인 후 필요한 항목을 바꾸세요.");
}
static bool Commit(void) {
    StopCapture(NULL);
    for(int p=0;p<8;++p) {
        for(int a=0;a<ActionCount;++a) { KeyAt(Keys_Def[p],a)=UI.keyboard[p][a];PadBinding[p][a]=UI.pad[p][a]; }
        *TypeAt(p)=UI.types[p];PadDevice[p]=UI.device[p];
    }
    Deadzone=UI.deadzone;StickDirections=UI.stickDirections;Make_IO_Table();Input_ClearState();
    if(!ConfigFile[0]) {
        wchar_t path[1024];GetModuleFileNameW(NULL,path,1024);
        WideCharToMultiByte(CP_UTF8,0,path,-1,ConfigFile,sizeof(ConfigFile),NULL,NULL);
        char *slash=strrchr(ConfigFile,'\\');if(slash) strcpy(slash+1,"Gens.cfg");else strcpy(ConfigFile,"Gens.cfg");
    }
    bool saved=SaveInput(ConfigFile);
    Status(saved?L"입력 설정을 적용하고 저장했습니다.":L"설정은 적용했지만 파일에 저장하지 못했습니다. 쓰기 가능한 폴더에서 실행해 주세요.");
    return saved;
}

static void BuildEditor(HWND hwnd) {
    UI.hwnd=hwnd;SettingsWindow=hwnd;UI.player=0;UI.row=0;UI.column=1;UI.captureRow=-1;
    UI.deadzone=Deadzone;PreviewDeadzone=Deadzone;UI.devicePlayer=-1;
    UI.stickDirections=StickDirections;PreviewStickDirections=StickDirections?1:0;
    HDC dc=GetDC(hwnd);UI.dpi=GetDeviceCaps(dc,LOGPIXELSX);ReleaseDC(hwnd,dc);
    if(UI.dpi<96) UI.dpi=96;
    MONITORINFO monitor;memset(&monitor,0,sizeof(monitor));monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(GetParent(hwnd),MONITOR_DEFAULTTONEAREST),&monitor);
    int available=monitor.rcWork.bottom-monitor.rcWork.top-60;
    if(Scale(628)>available) UI.dpi=available*96/628;
    if(UI.dpi<72) UI.dpi=72;
    UI.font=CreateFontW(-MulDiv(10,UI.dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"맑은 고딕");
    UI.titleFont=CreateFontW(-MulDiv(15,UI.dpi,72),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"맑은 고딕");
    RECT rect={0,0,Scale(780),Scale(628)};
    AdjustWindowRectEx(&rect,WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME,FALSE,WS_EX_DLGMODALFRAME);
    int width=rect.right-rect.left,height=rect.bottom-rect.top;
    int x=monitor.rcWork.left+((monitor.rcWork.right-monitor.rcWork.left)-width)/2;
    int y=monitor.rcWork.top+((monitor.rcWork.bottom-monitor.rcWork.top)-height)/2;
    SetWindowPos(hwnd,NULL,x,y,width,height,SWP_NOZORDER);
    SetWindowTextW(hwnd,L"키보드 · 게임패드 설정");
    HWND heading=AddControl(L"STATIC",L"내 손에 맞는 입력 설정",0,ID_HEADING,20,14,720,32);
    SendMessageW(heading,WM_SETFONT,(WPARAM)UI.titleFont,TRUE);
    AddControl(L"STATIC",L"플레이어",0,0,20,57,120,22);
    AddControl(L"STATIC",L"게임패드 장치",0,0,194,57,330,22);
    AddControl(L"STATIC",L"게임에서 사용할 패드",0,0,594,57,170,22);
    HWND player=AddControl(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,ID_PLAYER,20,81,158,230);
    const wchar_t *players[]={L"1P",L"2P",L"1P 멀티탭 B",L"1P 멀티탭 C",L"1P 멀티탭 D",L"2P 멀티탭 B",L"2P 멀티탭 C",L"2P 멀티탭 D"};
    for(int p=0;p<8;++p) SendMessageW(player,CB_ADDSTRING,0,(LPARAM)players[p]);
    SendMessageW(player,CB_SETCURSEL,0,0);
    AddControl(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,ID_DEVICE,194,81,336,240);
    AddControl(L"BUTTON",L"검색",WS_TABSTOP|BS_PUSHBUTTON,ID_SCAN,538,80,48,29);
    HWND type=AddControl(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|WS_TABSTOP,ID_TYPE,594,81,166,140);
    SendMessageW(type,CB_ADDSTRING,0,(LPARAM)L"3버튼");SendMessageW(type,CB_ADDSTRING,0,(LPARAM)L"6버튼");
    AddControl(L"BUTTON",L"1번 포트 멀티탭",WS_TABSTOP|BS_AUTOCHECKBOX,ID_PORT1,20,119,178,26);
    AddControl(L"BUTTON",L"2번 포트 멀티탭",WS_TABSTOP|BS_AUTOCHECKBOX,ID_PORT2,204,119,178,26);
    AddControl(L"BUTTON",L"XInput 왼쪽 스틱으로도 이동",WS_TABSTOP|BS_AUTOCHECKBOX,ID_STICK,398,119,362,26);
    CheckDlgButton(hwnd,ID_STICK,UI.stickDirections?BST_CHECKED:BST_UNCHECKED);
    UI.list=AddControl(WC_LISTVIEWW,L"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER,ID_LIST,20,156,740,286,WS_EX_CLIENTEDGE);
    SendMessageW(UI.list,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES);
    const wchar_t *heads[]={L"게임 버튼",L"키보드",L"게임패드",L"입력 확인"};
    const int widths[]={100,178,325,116};
    for(int c=0;c<4;++c) {
        LVCOLUMNW column;memset(&column,0,sizeof(column));column.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;
        column.pszText=(LPWSTR)heads[c];column.cx=Scale(widths[c]);column.iSubItem=c;
        SendMessageW(UI.list,LVM_INSERTCOLUMNW,c,(LPARAM)&column);
    }
    for(int r=0;r<ActionCount;++r) {
        LVITEMW item;memset(&item,0,sizeof(item));item.mask=LVIF_TEXT;item.iItem=r;item.pszText=(LPWSTR)L"";
        SendMessageW(UI.list,LVM_INSERTITEMW,0,(LPARAM)&item);
    }
    AddControl(L"BUTTON",L"키보드 지정",WS_TABSTOP|BS_PUSHBUTTON,ID_KEY_CAPTURE,20,452,136,32);
    AddControl(L"BUTTON",L"게임패드 지정",WS_TABSTOP|BS_PUSHBUTTON,ID_PAD_CAPTURE,164,452,144,32);
    AddControl(L"BUTTON",L"선택 할당 해제",WS_TABSTOP|BS_PUSHBUTTON,ID_CLEAR,316,452,144,32);
    AddControl(L"BUTTON",L"일괄 설정",WS_TABSTOP|BS_PUSHBUTTON,ID_SEQUENCE,468,452,144,32);
    AddControl(L"STATIC",L"Esc: 할당 취소",0,0,624,458,136,25);
    AddControl(L"BUTTON",L"키보드 기본값",WS_TABSTOP|BS_PUSHBUTTON,ID_KEY_DEFAULT,20,495,136,30);
    AddControl(L"BUTTON",L"패드 기본값",WS_TABSTOP|BS_PUSHBUTTON,ID_PAD_DEFAULT,164,495,144,30);
    AddControl(L"STATIC",L"스틱 데드존",0,0,332,500,114,24);
    HWND slider=AddControl(TRACKBAR_CLASSW,L"",WS_TABSTOP|TBS_HORZ|TBS_AUTOTICKS,ID_DEADZONE,442,494,240,32);
    SendMessageW(slider,TBM_SETRANGE,TRUE,MAKELONG(10,60));SendMessageW(slider,TBM_SETTICFREQ,10,0);
    SendMessageW(slider,TBM_SETPOS,TRUE,UI.deadzone);
    wchar_t dead[32];swprintf(dead,32,L"%d%%",UI.deadzone);AddControl(L"STATIC",dead,0,ID_DEAD_LABEL,696,500,64,24);
    UI.status=AddControl(L"STATIC",L"",0,ID_STATUS,20,536,740,42);
    AddControl(L"BUTTON",L"적용",WS_TABSTOP|BS_PUSHBUTTON,ID_APPLY,422,584,94,32);
    AddControl(L"BUTTON",L"저장 후 닫기",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,524,584,134,32);
    AddControl(L"BUTTON",L"취소",WS_TABSTOP|BS_PUSHBUTTON,IDCANCEL,666,584,94,32);
    for(int p=0;p<8;++p) {
        for(int a=0;a<ActionCount;++a) { UI.keyboard[p][a]=KeyAt(Keys_Def[p],a);UI.pad[p][a]=PadBinding[p][a]; }
        UI.device[p]=PadDevice[p];UI.types[p]=*TypeAt(p);
    }
    CheckDlgButton(hwnd,ID_PORT1,(UI.types[0]&0x10)?BST_CHECKED:BST_UNCHECKED);
    CheckDlgButton(hwnd,ID_PORT2,(UI.types[1]&0x10)?BST_CHECKED:BST_UNCHECKED);
    Init_Input(GetModuleHandleW(NULL),hwnd);ModernInput_Poll(TRUE);
    UpdatePlayer();SetTimer(hwnd,1,20,NULL);UI.nextNames=GetTickCount()+2000;
    SetFocus(player);
}

static INT_PTR CALLBACK EditorProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
    switch(message) {
    case WM_INITDIALOG:BuildEditor(hwnd);return FALSE;
    case WM_TIMER:
        Update_Input();
        if(SuppressDialogKeys && !Check_Key_Pressed(DIK_ESCAPE) && !Check_Key_Pressed(DIK_RETURN) && !Check_Key_Pressed(DIK_SPACE)) SuppressDialogKeys=false;
        CaptureTick();LiveTick();
        if(!Capturing && (LONG)(GetTickCount()-UI.nextNames)>=0) { UpdateDeviceList();UI.nextNames=GetTickCount()+2000; }
        return TRUE;
    case WM_SYSKEYDOWN:
    case WM_SYSCHAR:
        if(Capturing) return TRUE;
        break;
    case WM_SYSCOMMAND:
        if(Capturing && (wParam&0xFFF0)==SC_KEYMENU) return TRUE;
        break;
    case WM_ACTIVATE:
        if(LOWORD(wParam)==WA_INACTIVE) { Input_ClearState();UI.releaseTicks=0; }
        return FALSE;
    case WM_HSCROLL:
        if((HWND)lParam==GetDlgItem(hwnd,ID_DEADZONE)) {
            UI.deadzone=(int)SendMessageW((HWND)lParam,TBM_GETPOS,0,0);PreviewDeadzone=UI.deadzone;
            wchar_t value[32];swprintf(value,32,L"%d%%",UI.deadzone);SetDlgItemTextW(hwnd,ID_DEAD_LABEL,value);
            Status(L"스틱을 놓았는데도 입력되면 데드존을 높이세요. 방향키와 일반 버튼에는 영향을 주지 않습니다.");return TRUE;
        }break;
    case WM_NOTIFY: {
        NMHDR *nm=(NMHDR*)lParam;
        if(nm->idFrom==ID_LIST) {
            if(nm->code==LVN_ITEMCHANGED) {
                NMLISTVIEW *lv=(NMLISTVIEW*)nm;if((lv->uNewState&LVIS_SELECTED) && lv->iItem>=0) UI.row=lv->iItem;
            } else if(nm->code==NM_CLICK || nm->code==NM_DBLCLK) {
                NMITEMACTIVATE *item=(NMITEMACTIVATE*)nm;
                if(item->iItem>=0) { UI.row=item->iItem;if(item->iSubItem==1||item->iSubItem==2) UI.column=item->iSubItem; }
                if(nm->code==NM_DBLCLK && item->iItem>=0) BeginCapture(UI.column,false);
            } else if(nm->code==LVN_KEYDOWN) {
                NMLVKEYDOWN *key=(NMLVKEYDOWN*)nm;
                if(!Capturing && key->wVKey==VK_DELETE) ClearSelected();
                else if(!Capturing && key->wVKey==VK_RETURN) BeginCapture(UI.column,false);
            } else if(nm->code==NM_CUSTOMDRAW) {
                NMLVCUSTOMDRAW *draw=(NMLVCUSTOMDRAW*)nm;
                if(draw->nmcd.dwDrawStage==CDDS_PREPAINT) { SetWindowLongPtrW(hwnd,DWLP_MSGRESULT,CDRF_NOTIFYITEMDRAW);return TRUE; }
                if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) {
                    int row=(int)draw->nmcd.dwItemSpec;
                    if(row>=0 && row<ActionCount) {
                        int a=RowAction[row];
                        if(!ActionAvailable(a)) draw->clrText=RGB(128,128,128);
                        else if(UI.lastPressed[row]>0) draw->clrText=RGB(0,120,62);
                        else if(IsDuplicate(a,1)||IsDuplicate(a,2)) draw->clrText=RGB(170,91,0);
                    }
                    SetWindowLongPtrW(hwnd,DWLP_MSGRESULT,CDRF_NEWFONT);return TRUE;
                }
            }
        }break;
    }
    case WM_COMMAND: {
        int id=LOWORD(wParam),notify=HIWORD(wParam);
        if(id==IDCANCEL) {
            if(SuppressDialogKeys && !Capturing) return TRUE;
            if(Capturing) { StopCapture(L"할당을 취소했습니다.");return TRUE; }
            EndDialog(hwnd,IDCANCEL);return TRUE;
        }
        if(id==IDOK) { if(Capturing || SuppressDialogKeys) return TRUE;if(Commit()) EndDialog(hwnd,IDOK);return TRUE; }
        if(id==ID_APPLY) { Commit();return TRUE; }
        if(Capturing && id!=ID_CLEAR) return TRUE;
        if(id==ID_PLAYER && notify==CBN_SELCHANGE) {
            UI.player=(int)SendDlgItemMessageW(hwnd,ID_PLAYER,CB_GETCURSEL,0,0);UpdatePlayer();return TRUE;
        }
        if(id==ID_DEVICE && notify==CBN_SELCHANGE) {
            int selected=(int)SendDlgItemMessageW(hwnd,ID_DEVICE,CB_GETCURSEL,0,0);
            unsigned value=(unsigned)SendDlgItemMessageW(hwnd,ID_DEVICE,CB_GETITEMDATA,selected,0);
            unsigned old=UI.device[UI.player];UI.device[UI.player]=value;
            for(int a=0;a<ActionCount;++a) {
                unsigned &binding=UI.pad[UI.player][a];
                if(!value) binding=0;
                else if((value&0xF000)==(old&0xF000) && binding) binding=value|(binding&255);
                else binding=0;
            }
            UpdateRows();Status(L"게임패드 장치를 선택했습니다. ‘패드 기본값’ 또는 ‘게임패드 지정’으로 버튼을 설정하세요.");return TRUE;
        }
        if(id==ID_TYPE && notify==CBN_SELCHANGE) {
            unsigned type=(unsigned)SendDlgItemMessageW(hwnd,ID_TYPE,CB_GETCURSEL,0,0);
            UI.types[UI.player]=(UI.types[UI.player]&~1u)|(type&1);UpdateRows();return TRUE;
        }
        if(id==ID_STICK) {
            UI.stickDirections=IsDlgButtonChecked(hwnd,ID_STICK)!=0;PreviewStickDirections=UI.stickDirections?1:0;
            UpdateRows();return TRUE;
        }
        if(id==ID_PORT1||id==ID_PORT2) {
            int p=id==ID_PORT1?0:1;UI.types[p]=(UI.types[p]&~0x10u)|(IsDlgButtonChecked(hwnd,id)?0x10u:0u);return TRUE;
        }
        switch(id) {
        case ID_KEY_CAPTURE:BeginCapture(1,false);return TRUE;
        case ID_PAD_CAPTURE:BeginCapture(2,false);return TRUE;
        case ID_SEQUENCE:SelectRow(0);BeginCapture(UI.column,true);return TRUE;
        case ID_CLEAR:ClearSelected();return TRUE;
        case ID_KEY_DEFAULT:DefaultKeyboard();return TRUE;
        case ID_PAD_DEFAULT:DefaultPad();return TRUE;
        case ID_SCAN:
            Input_RefreshDevices();for(int i=0;i<4;++i) XNextScan[i]=0;ModernInput_Poll(Input_HasFocus());
            UpdateDeviceList();Status(L"연결된 게임패드를 다시 검색했습니다.");return TRUE;
        }
        break;
    }
    case WM_CLOSE:EndDialog(hwnd,IDCANCEL);return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd,1);End_Input();Capturing=false;SettingsWindow=NULL;PreviewDeadzone=-1;PreviewStickDirections=-1;
        if(UI.font) DeleteObject(UI.font);
        if(UI.titleFont) DeleteObject(UI.titleFont);
        return TRUE;
    }
    return FALSE;
}

INT_PTR ModernInput_ShowDialog(HWND owner) {
    if(SettingsWindow) { SetForegroundWindow(SettingsWindow);return 0; }
    if(!Loaded) {
        for(int p=0;p<2;++p) for(int a=0;a<ActionCount;++a) PadBinding[p][a]=XDefault(a,p);
        Loaded=true;
    }
    INITCOMMONCONTROLSEX common;common.dwSize=sizeof(common);common.dwICC=ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES;
    InitCommonControlsEx(&common);
    memset(&UI,0,sizeof(UI));SuppressDialogKeys=false;
    struct EmptyDialog { DLGTEMPLATE dialog;WORD menu,windowClass;WCHAR title; } layout;
    memset(&layout,0,sizeof(layout));
    layout.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;
    layout.dialog.dwExtendedStyle=WS_EX_DLGMODALFRAME;
    layout.dialog.cx=390;layout.dialog.cy=314;
    return DialogBoxIndirectParamW(GetModuleHandleW(NULL),&layout.dialog,owner,EditorProc,0);
}
