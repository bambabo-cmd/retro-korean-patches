/* Gens KO, 2026-11-20. No display-mode switching or global desktop settings. GPLv2. */
#include "ko_winapi.h"
#include "desktop_shell.h"
#include "renderer_gl.h"
#include "modern_input.h"
#include "hotkeys.h"
#include "playback.h"
#include "G_Input.h"
#include "G_main.h"
#include "rom.h"
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <string>
#include <vector>
#include <cstring>

static bool menu_visible=false,alt_armed=false,esc_handled=false,initialized=false,was_full=false;
static RECT saved_rect={100,100,1060,820};
static std::wstring launch_rom,launch_shader;
static int launch_full=-1;

static std::wstring absolute_path(const wchar_t *s){
    DWORD n=GetFullPathNameW(s,0,NULL,NULL);if(!n||n>32768)return std::wstring();
    std::vector<wchar_t>v((size_t)n+1);if(!GetFullPathNameW(s,(DWORD)v.size(),v.data(),NULL))return std::wstring();return v.data();
}
void Desktop_PreInit(void){
    HMODULE user=GetModuleHandleW(L"user32.dll");
    typedef BOOL(WINAPI *SetDpi)(HANDLE);
    SetDpi setdpi=(SetDpi)GetProcAddress(user,"SetProcessDpiAwarenessContext");
    if(setdpi)setdpi((HANDLE)(INT_PTR)-4);else SetProcessDPIAware();
    int count=0;LPWSTR *args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(args){
        for(int i=1;i<count;++i){
            std::wstring arg=args[i];
            if(arg==L"--fullscreen"||arg==L"-f")launch_full=1;
            else if(arg==L"--windowed")launch_full=0;
            else if(arg==L"--show-menu")menu_visible=true;
            else if(arg==L"--hide-menu")menu_visible=false;
            else if(arg==L"--shader"&&i+1<count)launch_shader=absolute_path(args[++i]);
            else if(arg==L"--"){if(i+1<count)launch_rom=absolute_path(args[++i]);break;}
            else if(!arg.empty()&&arg[0]!=L'-'&&launch_rom.empty())launch_rom=absolute_path(arg.c_str());
        }
        LocalFree(args);
    }
    KoInitializePaths();
}
void Desktop_LoadConfig(const char *path){
    int x=KoGetPrivateProfileInt("Window KO","X",100,path),y=KoGetPrivateProfileInt("Window KO","Y",100,path);
    int w=KoGetPrivateProfileInt("Window KO","Width",960,path),h=KoGetPrivateProfileInt("Window KO","Height",740,path);
    if(w<320||w>16384)w=960;if(h<240||h>16384)h=740;
    saved_rect={x,y,x+w,y+h};
}
void Desktop_SaveConfig(const char *path){
    RECT r=saved_rect;if(HWnd&&!was_full&&!IsIconic(HWnd))GetWindowRect(HWnd,&r);
    const char *keys[]={"X","Y","Width","Height"};int values[]={r.left,r.top,r.right-r.left,r.bottom-r.top};
    for(int i=0;i<4;++i){char b[32];snprintf(b,sizeof(b),"%d",values[i]);KoWritePrivateProfileString("Window KO",keys[i],b,path);}
}
static RECT visible_rect(RECT r){
    HMONITOR mon=MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST);MONITORINFO mi={sizeof(mi)};
    if(!GetMonitorInfoW(mon,&mi))return r;
    LONG w=std::min(r.right-r.left,mi.rcWork.right-mi.rcWork.left),h=std::min(r.bottom-r.top,mi.rcWork.bottom-mi.rcWork.top);
    LONG x=std::max(mi.rcWork.left,std::min(r.left,mi.rcWork.right-w));
    LONG y=std::max(mi.rcWork.top,std::min(r.top,mi.rcWork.bottom-h));
    return RECT{x,y,x+w,y+h};
}
void Desktop_ApplyMode(HWND hwnd,int full,int force){
    if(!hwnd)return;
    bool target=full!=0;
    if(target&&!was_full){if(initialized&&!IsIconic(hwnd)){GetWindowRect(hwnd,&saved_rect);}}
    if(target){
        MONITORINFO mi={sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
        SetWindowLongPtrW(hwnd,GWL_STYLE,WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS);
        SetWindowPos(hwnd,NULL,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }else if(!initialized||was_full||force){
        SetWindowLongPtrW(hwnd,GWL_STYLE,WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS);
        RECT r=visible_rect(saved_rect);
        SetWindowPos(hwnd,NULL,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
    }
    was_full=target;initialized=true;
    Renderer_Resize(hwnd);
}
void Desktop_ApplyMenu(HWND hwnd,HMENU menu){
    SetMenu(hwnd,menu_visible?menu:NULL);DrawMenuBar(hwnd);
    if(initialized)Renderer_Resize(hwnd);
}
bool Desktop_IsMenuVisible(void){return menu_visible;}
bool Desktop_IsInitialized(void){return initialized;}
void Desktop_ToggleMenu(HWND hwnd){
    Playback_Suspend();
    RECT c={},r={};GetClientRect(hwnd,&c);GetWindowRect(hwnd,&r);
    menu_visible=!menu_visible;SetMenu(hwnd,menu_visible?Gens_Menu:NULL);DrawMenuBar(hwnd);
    if(!was_full&&!IsZoomed(hwnd)&&!IsIconic(hwnd)){
        RECT target={0,0,c.right,c.bottom};
        typedef BOOL(WINAPI *AdjustDpi)(LPRECT,DWORD,BOOL,DWORD,UINT);
        typedef UINT(WINAPI *GetDpi)(HWND);
        HMODULE u=GetModuleHandleW(L"user32.dll");
        AdjustDpi adjust=(AdjustDpi)GetProcAddress(u,"AdjustWindowRectExForDpi");GetDpi dpi=(GetDpi)GetProcAddress(u,"GetDpiForWindow");
        DWORD style=(DWORD)GetWindowLongPtrW(hwnd,GWL_STYLE),ex=(DWORD)GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
        if(adjust&&dpi)adjust(&target,style,menu_visible,ex,dpi(hwnd));else AdjustWindowRectEx(&target,style,menu_visible,ex);
        SetWindowPos(hwnd,NULL,0,0,target.right-target.left,target.bottom-target.top,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    Input_ClearState();Renderer_Resize(hwnd);InvalidateRect(hwnd,NULL,FALSE);
}
void Desktop_OnDeactivate(void){alt_armed=false;esc_handled=false;Playback_Suspend();Input_ClearState();}
static bool held_modifier(bool include_alt){
    const int keys[]={VK_CONTROL,VK_SHIFT,VK_LWIN,VK_RWIN};
    for(int key:keys)if(GetKeyState(key)&0x8000)return true;
    return include_alt&&(GetKeyState(VK_MENU)&0x8000);
}
bool Desktop_FilterMessage(MSG *m){
    if(!m||m->hwnd!=HWnd||ModernInput_IsCapturing()||Hotkeys_IsCapturing())return false;
    bool down=m->message==WM_KEYDOWN||m->message==WM_SYSKEYDOWN;
    bool up=m->message==WM_KEYUP||m->message==WM_SYSKEYUP;
    if(down&&m->wParam==VK_MENU){if(!(m->lParam&(1L<<30)))alt_armed=!held_modifier(false);return true;}
    if(down&&m->wParam!=VK_MENU)alt_armed=false;
    if(up&&m->wParam==VK_MENU){bool toggle=alt_armed;alt_armed=false;if(toggle)Desktop_ToggleMenu(HWnd);return true;}
    if((down||up)&&m->wParam==VK_ESCAPE){
        if(down){
            if(!(m->lParam&(1L<<30))){esc_handled=!held_modifier(true);if(esc_handled)Desktop_ToggleMenu(HWnd);}
            return esc_handled;
        }
        bool handled=esc_handled;esc_handled=false;return handled;
    }
    return false;
}
void Desktop_OnDpiChanged(HWND hwnd,LPARAM value){
    if(was_full){Desktop_ApplyMode(hwnd,1,0);return;}
    RECT *r=(RECT*)value;if(r)SetWindowPos(hwnd,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
    Renderer_Resize(hwnd);
}
void Desktop_LaunchContent(HWND owner){
    if(launch_full>=0)Set_Render(owner,launch_full,-1,FALSE);
    if(!launch_rom.empty()){
        std::string p=KoUtf8(launch_rom.c_str());
        if(p.size()>=1024)MessageBoxW(owner,L"ROM 경로가 너무 깁니다. 더 짧은 폴더로 옮겨 주세요.",L"Gens KO",MB_OK|MB_ICONERROR);
        else Pre_Load_Rom(owner,const_cast<char*>(p.c_str()));
    }
    if(!launch_shader.empty())Renderer_LoadPreset(launch_shader.c_str(),owner);
    Desktop_ApplyMenu(owner,Gens_Menu);
    Renderer_SetOverlayUtf8("Alt / Esc: 메뉴 표시 · 숨기기",3500);
}
void Desktop_CreateShortcut(HWND owner){
    if(!Current_Rom_Path[0]){MessageBoxW(owner,L"먼저 바로가기를 만들 게임을 실행해 주세요.",L"게임 바로가기",MB_OK|MB_ICONINFORMATION);return;}
    std::wstring rom=absolute_path(KoWide(Current_Rom_Path).c_str());
    if(rom.empty()||GetFileAttributesW(rom.c_str())==INVALID_FILE_ATTRIBUTES){MessageBoxW(owner,L"현재 ROM 파일을 찾을 수 없습니다.",L"게임 바로가기",MB_OK|MB_ICONERROR);return;}
    wchar_t exe[32768],desktop[MAX_PATH]={},out[32768]={};GetModuleFileNameW(NULL,exe,32768);SHGetFolderPathW(owner,CSIDL_DESKTOPDIRECTORY,NULL,SHGFP_TYPE_CURRENT,desktop);
    size_t sep=rom.find_last_of(L"\\/");std::wstring name=rom.substr(sep==std::wstring::npos?0:sep+1);size_t dot=name.find_last_of(L'.');if(dot!=std::wstring::npos)name.resize(dot);name+=L".lnk";
    wcsncpy(out,name.c_str(),32767);OPENFILENAMEW ofn={sizeof(ofn)};ofn.hwndOwner=owner;ofn.lpstrFile=out;ofn.nMaxFile=32768;ofn.lpstrInitialDir=desktop;ofn.lpstrDefExt=L"lnk";ofn.lpstrTitle=L"게임 바로가기 저장";ofn.lpstrFilter=L"Windows 바로가기 (*.lnk)\0*.lnk\0\0";ofn.Flags=OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&ofn))return;
    HRESULT init=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);IShellLinkW *link=NULL;
    HRESULT hr=CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLinkW,(void**)&link);
    if(SUCCEEDED(hr)){
        std::wstring args=L"--windowed --hide-menu -- \""+rom+L"\"";std::wstring wd=exe;size_t p=wd.find_last_of(L'\\');if(p!=std::wstring::npos)wd.resize(p);
        link->SetPath(exe);link->SetArguments(args.c_str());link->SetWorkingDirectory(wd.c_str());link->SetIconLocation(exe,0);link->SetDescription(L"Gens KO로 게임 실행");link->SetShowCmd(SW_SHOWNORMAL);
        IPersistFile *persist=NULL;hr=link->QueryInterface(IID_IPersistFile,(void**)&persist);if(SUCCEEDED(hr)){hr=persist->Save(out,TRUE);persist->Release();}link->Release();
    }
    if(SUCCEEDED(init))CoUninitialize();
    if(FAILED(hr))MessageBoxW(owner,L"바로가기를 저장하지 못했습니다. 폴더의 쓰기 권한을 확인해 주세요.",L"게임 바로가기",MB_OK|MB_ICONERROR);
    else Renderer_SetOverlayUtf8("게임 바로가기를 저장했습니다.",2500);
}
void Desktop_ShowHelp(HWND owner){
    MessageBoxW(owner,
        L"Alt / Esc: 메뉴 표시 · 숨기기\nAlt+Enter: 창 / 전체 화면 전환\n"
        L"Tab: 빨리 감기 (기본: 홀드)\nBackspace: 되감기 (기본: 홀드)\n"
        L"Space: 슬로우모션 (기본: 토글)\nPause: 일시정지 / 재개\n"
        L"Ctrl+R: 게임 리셋\nCtrl+O: ROM 열기\nF5 / F8: 빠른 저장 / 불러오기\n"
        L"F6 / F7: 저장 슬롯 변경\n\n"
        L"설정 → 핫키 / 배속 / 되감기 설정: 키, 홀드·토글, 속도와 기록 용량\n"
        L"설정 → 키보드 / 게임패드: 입력 장치와 버튼 할당\n"
        L"기본 6버튼: ABC=CXZ / XYZ=DSA / START=V / MODE=F\n"
        L"화면 → 셰이더 설정: 효과와 외부 GLSL 프리셋\n"
        L"파일 → 게임 바로가기 만들기: 현재 ROM의 Windows 바로가기\n\n"
        L"게임 파일을 창에 끌어 놓아 실행할 수도 있습니다.\n"
        L"전체 화면은 현재 모니터를 사용하는 테두리 없는 방식입니다.",
        L"Gens KO 사용 안내",MB_OK|MB_ICONINFORMATION);
}
