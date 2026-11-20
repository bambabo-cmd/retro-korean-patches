/* Gens KO production-settings component probe, 2026-11-20. GPL-2.0.
 * Copyright notices and GPL terms for the Gens input frontend are preserved
 * in LICENSE.txt. This harness contains no emulation core, game ROM or BIOS.
 * It executes the unchanged production keyboard/gamepad and hotkey dialogs.
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "G_Input.h"
#include "G_main.h"
#include "io.h"
#include "modern_input.h"
#include "hotkeys.h"

HINSTANCE ghInstance=NULL;
HWND HWnd=NULL;
int Net_Play=0;

/* Only the controller's exported variables are supplied. The real DirectInput
   and XInput implementations remain in G_dinput.cpp / modern_input.cpp.
   No CPU, memory map, renderer, audio backend, or game code is linked. */
extern "C" {
#define PROBE_CONTROLLER(p) \
    unsigned Controller_##p##_Type=1; \
    unsigned Controller_##p##_Up=1,Controller_##p##_Down=1; \
    unsigned Controller_##p##_Left=1,Controller_##p##_Right=1; \
    unsigned Controller_##p##_Start=1,Controller_##p##_Mode=1; \
    unsigned Controller_##p##_A=1,Controller_##p##_B=1,Controller_##p##_C=1; \
    unsigned Controller_##p##_X=1,Controller_##p##_Y=1,Controller_##p##_Z=1
PROBE_CONTROLLER(1);PROBE_CONTROLLER(2);PROBE_CONTROLLER(1B);PROBE_CONTROLLER(1C);
PROBE_CONTROLLER(1D);PROBE_CONTROLLER(2B);PROBE_CONTROLLER(2C);PROBE_CONTROLLER(2D);
#undef PROBE_CONTROLLER
/* Controller handshakes only matter while a game is running, which this
   component probe deliberately cannot do. The UI updates its real variables. */
void Make_IO_Table(void) {}
}

namespace {
const wchar_t*ClassName=L"GensKoSettingsProbe";
const wchar_t*WindowTitle=L"Gens KO 설정 UI 검증";
char ConfigPath[1024]={0};
HFONT BodyFont=NULL;
enum {InputCommand=40183,HotkeyCommand=57005};

void ConfigFile() {
    wchar_t path[1024];DWORD count=GetModuleFileNameW(NULL,path,1024);
    if(!count||count>=1000)return;
    wchar_t*slash=wcsrchr(path,L'\\');
    if(!slash)return;
    wcscpy(slash+1,L"Gens.cfg");
    WideCharToMultiByte(CP_UTF8,0,path,-1,ConfigPath,sizeof(ConfigPath),NULL,NULL);
    const wchar_t*players[8]={L"P1",L"P2",L"P1B",L"P1C",L"P1D",L"P2B",L"P2C",L"P2D"};
    const wchar_t*actions[12]={L"Start",L"Mode",L"A",L"B",L"C",L"X",L"Y",L"Z",L"Up",L"Down",L"Left",L"Right"};
    unsigned*types[8]={&Controller_1_Type,&Controller_2_Type,&Controller_1B_Type,&Controller_1C_Type,
        &Controller_1D_Type,&Controller_2B_Type,&Controller_2C_Type,&Controller_2D_Type};
    for(int p=0;p<8;p++) {
        wchar_t key[64];swprintf(key,64,L"%ls.Type",players[p]);
        *types[p]=GetPrivateProfileIntW(L"Input",key,1,path)&(p<2?0x13:0x03);
        unsigned*bindings[12]={&Keys_Def[p].Start,&Keys_Def[p].Mode,&Keys_Def[p].A,&Keys_Def[p].B,
            &Keys_Def[p].C,&Keys_Def[p].X,&Keys_Def[p].Y,&Keys_Def[p].Z,&Keys_Def[p].Up,
            &Keys_Def[p].Down,&Keys_Def[p].Left,&Keys_Def[p].Right};
        for(int a=0;a<12;a++) {
            swprintf(key,64,L"%ls.%ls",players[p],actions[a]);
            *bindings[a]=GetPrivateProfileIntW(L"Input",key,*bindings[a],path);
        }
    }
    ModernInput_LoadConfig(ConfigPath);Hotkeys_LoadConfig(ConfigPath);
}
HWND Add(HWND owner,const wchar_t*kind,const wchar_t*text,DWORD style,int id,
         int x,int y,int width,int height) {
    HWND child=CreateWindowExW(0,kind,text,WS_CHILD|WS_VISIBLE|style,x,y,width,height,
                              owner,(HMENU)(INT_PTR)id,ghInstance,NULL);
    SendMessageW(child,WM_SETFONT,(WPARAM)BodyFont,TRUE);return child;
}
LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) {
    switch(message) {
    case WM_CREATE:
        BodyFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        Add(window,L"STATIC",L"Gens KO 실제 설정 창",SS_LEFT,0,28,26,700,30);
        Add(window,L"STATIC",L"동일한 제품 소스의 설정 창과 입력 처리를 실행합니다.\r\n"
            L"게임 실행 및 에뮬레이션 코어를 포함하지 않는 구성 요소 검증 프로그램입니다.",SS_LEFT,
            0,28,70,700,66);
        Add(window,L"BUTTON",L"키보드 · 게임패드 설정",WS_TABSTOP|BS_PUSHBUTTON,InputCommand,28,160,325,48);
        Add(window,L"BUTTON",L"핫키 · 재생 속도 설정",WS_TABSTOP|BS_PUSHBUTTON,HotkeyCommand,375,160,325,48);
        Add(window,L"STATIC",L"Gens 입력 프런트엔드 및 Gens KO 수정 소스: GNU GPL v2.\r\n"
            L"보증 없이 제공되며, 라이선스와 해당 소스는 배포 파일에 포함됩니다.",SS_LEFT,
            0,28,250,700,65);
        SetTimer(window,1,20,NULL);return 0;
    case WM_TIMER:
        Hotkeys_Poll();return 0;
    case WM_COMMAND:
        if(LOWORD(wParam)==InputCommand) {
            Hotkeys_ClearActive();End_Input();ModernInput_ShowDialog(window);
            Init_Input(ghInstance,window);return 0;
        }
        if(LOWORD(wParam)==HotkeyCommand) {
            Hotkeys_ShowDialog(window);return 0;
        }
        break;
    case WM_ACTIVATEAPP:
        if(!wParam) {Hotkeys_ClearActive();Input_ClearState();}
        return 0;
    case WM_CLOSE:DestroyWindow(window);return 0;
    case WM_DESTROY:
        KillTimer(window,1);ModernInput_SaveConfig(ConfigPath);Hotkeys_SaveConfig(ConfigPath);
        End_Input();if(BodyFont)DeleteObject(BodyFont);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int show) {
    ghInstance=instance;
    typedef BOOL(WINAPI*SetDpi)(HANDLE);
    HMODULE user=GetModuleHandleW(L"user32.dll");
    SetDpi setDpi=(SetDpi)GetProcAddress(user,"SetProcessDpiAwarenessContext");
    if(setDpi)setDpi((HANDLE)(INT_PTR)-4);else SetProcessDPIAware();
    INITCOMMONCONTROLSEX common={sizeof(common),ICC_WIN95_CLASSES|ICC_LISTVIEW_CLASSES|ICC_BAR_CLASSES};
    InitCommonControlsEx(&common);ConfigFile();
    WNDCLASSW klass={};klass.lpfnWndProc=WindowProc;klass.hInstance=instance;
    klass.hCursor=LoadCursorW(NULL,MAKEINTRESOURCEW(32512));klass.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    klass.lpszClassName=ClassName;
    if(!RegisterClassW(&klass))return 2;
    HWnd=CreateWindowExW(0,ClassName,WindowTitle,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        50,50,760,400,NULL,NULL,instance,NULL);
    if(!HWnd)return 3;
    ShowWindow(HWnd,show);UpdateWindow(HWnd);Init_Input(instance,HWnd);
    if(command&&wcsstr(command,L"--input"))PostMessageW(HWnd,WM_COMMAND,InputCommand,0);
    if(command&&wcsstr(command,L"--transport"))PostMessageW(HWnd,WM_COMMAND,HotkeyCommand,0);
    MSG message={};
    while(GetMessageW(&message,NULL,0,0)>0) {
        if(Hotkeys_FilterMessage(&message))continue;
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
