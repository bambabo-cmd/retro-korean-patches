/* Gens KO production-settings and renderer component probe, 2026-11-20. GPL-2.0.
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
#include <GL/gl.h>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "G_Input.h"
#include "G_main.h"
#include "io.h"
#include "modern_input.h"
#include "hotkeys.h"
#include "renderer_gl.h"
#include "shader_preset.h"

HINSTANCE ghInstance=NULL;
HWND HWnd=NULL;
int Net_Play=0;

/* Only the controller's exported variables are supplied. The real DirectInput
   and XInput implementations remain in G_dinput.cpp / modern_input.cpp.
   No CPU, memory map, audio backend, or game code is linked. The renderer is
   the unchanged production OpenGL/GDI implementation with an owned pattern. */
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
const wchar_t*WindowTitle=L"Gens KO 설정·셰이더 검증화면 (게임 실행 아님)";
char ConfigPath[1024]={0};
HFONT BodyFont=NULL;
HWND PreviewWindow=NULL,RendererLabel=NULL;
unsigned short TestPattern[320*240];
enum {InputCommand=40183,HotkeyCommand=57005,ShaderCommand=57001};
const UINT RealCrtLoadMessage=WM_APP+1;
bool RealCrtRequested=false,RealCrtLoaded=false,RealCrtFailed=false,ClosedNormally=false;
unsigned VerifiedPresentations=0,FrameColors=0;
int ParsedPasses=0,ParsedFeedback=-1,ParsedFloatPasses=0;
int ActivePasses=0,ActiveFeedback=-1,ParameterCount=0;
unsigned LastGlError=GL_NO_ERROR;
DWORD LastProofWrite=0;
std::wstring ExeDirectory,ProofPath,RealCrtPath;
std::string Diagnostic,GlVendor,GlRenderer,GlVersion,BaselineHash,FrameHash;

std::string Utf8(const std::wstring& value) {
    if(value.empty())return std::string();
    int bytes=WideCharToMultiByte(CP_UTF8,0,value.c_str(),(int)value.size(),NULL,0,NULL,NULL);
    std::string result(bytes,'\0');
    if(bytes)WideCharToMultiByte(CP_UTF8,0,value.c_str(),(int)value.size(),&result[0],bytes,NULL,NULL);
    return result;
}
std::string JsonString(const std::string& value) {
    std::ostringstream output;output<<'"';
    for(size_t i=0;i<value.size();++i) {
        unsigned char ch=(unsigned char)value[i];
        if(ch=='"'||ch=='\\')output<<'\\'<<(char)ch;
        else if(ch<32)output<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)ch<<std::dec;
        else output<<(char)ch;
    }
    output<<'"';return output.str();
}
void FailRealCrt(const std::string& reason) {
    if(!RealCrtFailed)Diagnostic=reason;
    RealCrtFailed=true;
}
const char* RealCrtStatus() {
    return RealCrtFailed?"FAIL":(VerifiedPresentations>=3?"PASS":"PENDING");
}
void WriteRealCrtProof() {
    if(!RealCrtRequested||ProofPath.empty())return;
    std::ostringstream output;
    output<<"{\n"
      <<"  \"schema\": 1,\n"
      <<"  \"status\": "<<JsonString(RealCrtStatus())<<",\n"
      <<"  \"process_id\": "<<GetCurrentProcessId()<<",\n"
      <<"  \"scope\": \"Production GLSL renderer on an original RGB565 test pattern; no emulation core or game\",\n"
      <<"  \"gameplay_verified\": false,\n"
      <<"  \"preset_file\": \"shaders/real-crt-phosphor/real-crt-phosphor.glslp\",\n"
      <<"  \"requested_passes\": 14,\n"
      <<"  \"parsed_passes\": "<<ParsedPasses<<",\n"
      <<"  \"active_passes\": "<<ActivePasses<<",\n"
      <<"  \"parsed_feedback_pass\": "<<ParsedFeedback<<",\n"
      <<"  \"active_feedback_pass\": "<<ActiveFeedback<<",\n"
      <<"  \"parsed_float_passes\": "<<ParsedFloatPasses<<",\n"
      <<"  \"parameter_count\": "<<ParameterCount<<",\n"
      <<"  \"preset_load_succeeded\": "<<(RealCrtLoaded?"true":"false")<<",\n"
      <<"  \"verified_presentations\": "<<VerifiedPresentations<<",\n"
      <<"  \"framebuffer_sampled_colors\": "<<FrameColors<<",\n"
      <<"  \"framebuffer_readback\": \"Actual OpenGL front buffer after production SwapBuffers\",\n"
      <<"  \"baseline_frame_fnv1a64\": "<<JsonString(BaselineHash)<<",\n"
      <<"  \"shader_frame_fnv1a64\": "<<JsonString(FrameHash)<<",\n"
      <<"  \"shader_output_differs_from_baseline\": "<<(!BaselineHash.empty()&&!FrameHash.empty()&&BaselineHash!=FrameHash?"true":"false")<<",\n"
      <<"  \"gl_error\": "<<LastGlError<<",\n"
      <<"  \"gl_vendor\": "<<JsonString(GlVendor)<<",\n"
      <<"  \"gl_renderer\": "<<JsonString(GlRenderer)<<",\n"
      <<"  \"gl_version\": "<<JsonString(GlVersion)<<",\n"
      <<"  \"renderer_status\": "<<JsonString(Utf8(Renderer_Status()))<<",\n"
      <<"  \"diagnostic\": "<<JsonString(Diagnostic)<<",\n"
      <<"  \"closed_normally\": "<<(ClosedNormally?"true":"false")<<"\n}\n";
    const std::string text=output.str();const std::wstring temporary=ProofPath+L".tmp";
    FILE* file=_wfopen(temporary.c_str(),L"wb");
    if(!file){FailRealCrt("Could not create the renderer proof JSON.");return;}
    bool written=fwrite(text.data(),1,text.size(),file)==text.size();
    if(fflush(file)!=0)written=false;
    if(fclose(file)!=0)written=false;
    bool replaced=false;
    if(written)for(int retry=0;retry<20;++retry) {
        if(MoveFileExW(temporary.c_str(),ProofPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){replaced=true;break;}
        DWORD failure=GetLastError();
        if(failure!=ERROR_SHARING_VIOLATION&&failure!=ERROR_ACCESS_DENIED)break;
        /* PowerShell can briefly hold the previous JSON open while polling. */
        Sleep(10);
    }
    if(!written||!replaced)FailRealCrt("Could not atomically replace the renderer proof JSON.");
    LastProofWrite=GetTickCount();
}
bool ReadFrontBuffer(std::string& hash,unsigned& colorCount) {
    if(!wglGetCurrentContext()){FailRealCrt("No current OpenGL context for framebuffer verification.");return false;}
    RECT client={};
    if(!GetClientRect(PreviewWindow,&client)||client.right<32||client.bottom<32||client.right>4096||client.bottom>4096) {
        FailRealCrt("Preview has no usable framebuffer dimensions.");return false;
    }
    const size_t bytes=(size_t)client.right*(size_t)client.bottom*3;
    std::vector<unsigned char> pixels(bytes);
    GLint previousBuffer=GL_BACK,previousAlignment=4;
    glGetIntegerv(GL_READ_BUFFER,&previousBuffer);glGetIntegerv(GL_PACK_ALIGNMENT,&previousAlignment);
    glReadBuffer(GL_FRONT);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,client.right,client.bottom,GL_RGB,GL_UNSIGNED_BYTE,&pixels[0]);
    glReadBuffer((GLenum)previousBuffer);glPixelStorei(GL_PACK_ALIGNMENT,previousAlignment);
    LastGlError=glGetError();
    if(LastGlError!=GL_NO_ERROR){FailRealCrt("OpenGL framebuffer readback reported an error.");return false;}
    unsigned long long checksum=14695981039346656037ULL;
    std::set<unsigned> colors;
    for(size_t i=0;i<bytes;i++) {checksum^=pixels[i];checksum*=1099511628211ULL;}
    for(size_t i=0;i<bytes;i+=3)colors.insert(((unsigned)pixels[i]<<16)|((unsigned)pixels[i+1]<<8)|pixels[i+2]);
    colorCount=(unsigned)colors.size();
    std::ostringstream encoded;encoded<<std::hex<<std::setw(16)<<std::setfill('0')<<checksum;hash=encoded.str();
    if(colorCount<8){FailRealCrt("The actual OpenGL front buffer is blank or lacks the owned test pattern.");return false;}
    return true;
}
void VerifyRealCrtFrame() {
    if(!RealCrtRequested||!RealCrtLoaded||RealCrtFailed)return;
    ActivePasses=Renderer_ActivePassCount();ActiveFeedback=Renderer_FeedbackPass();
    ParameterCount=Renderer_ParameterCount();
    const GLubyte* vendor=glGetString(GL_VENDOR);
    const GLubyte* renderer=glGetString(GL_RENDERER);
    const GLubyte* version=glGetString(GL_VERSION);
    GlVendor=vendor?(const char*)vendor:"";GlRenderer=renderer?(const char*)renderer:"";
    GlVersion=version?(const char*)version:"";LastGlError=glGetError();
    if(!wglGetCurrentContext()||GlVendor.empty()||GlRenderer.empty()||GlVersion.empty()||LastGlError!=GL_NO_ERROR)
        FailRealCrt("The renderer has no functioning OpenGL context after presentation.");
    if(ActivePasses!=14||ActiveFeedback!=ParsedFeedback||ParameterCount<=0)
        FailRealCrt("Active renderer pass, feedback or parameter metadata differs from the requested Real CRT graph.");
    if(wcscmp(Renderer_Status(),L"OpenGL · GLSL 14패스")!=0)
        FailRealCrt("The production renderer did not report a successfully drawn 14-pass GLSL frame.");
    /* Read the actual displayed buffer for the initial frames and each later
       proof checkpoint. Mere successful program compilation is not a pass. */
    const bool previewExposed=IsWindowEnabled(HWnd)&&GetForegroundWindow()==HWnd;
    if(!RealCrtFailed&&previewExposed&&(VerifiedPresentations<3||GetTickCount()-LastProofWrite>=1000)) {
        if(ReadFrontBuffer(FrameHash,FrameColors)&&FrameHash==BaselineHash)
            FailRealCrt("Real CRT produced the same framebuffer as the stock OpenGL baseline.");
    }
    if(!RealCrtFailed&&previewExposed&&VerifiedPresentations<1000000)++VerifiedPresentations;
    if(RealCrtFailed||VerifiedPresentations<=3||GetTickCount()-LastProofWrite>=1000)WriteRealCrtProof();
}
void LoadRealCrt() {
    if(!RealCrtRequested||RealCrtLoaded||RealCrtFailed)return;
    WriteRealCrtProof();
    FILE* preset=_wfopen(RealCrtPath.c_str(),L"rb");
    if(!preset){FailRealCrt("The bundled Real CRT preset is missing.");WriteRealCrtProof();return;}
    fseek(preset,0,SEEK_END);long length=ftell(preset);rewind(preset);
    if(length<=0||length>1024*1024){fclose(preset);FailRealCrt("Invalid bundled Real CRT preset size.");WriteRealCrtProof();return;}
    std::string text((size_t)length,'\0');size_t bytes=fread(&text[0],1,(size_t)length,preset);fclose(preset);
    KoShaderPreset parsed;std::string error;
    if(bytes!=(size_t)length||!KoParseShaderPreset(text,parsed,error)) {
        FailRealCrt("Could not parse the bundled preset: "+error);WriteRealCrtProof();return;
    }
    ParsedPasses=(int)parsed.passes.size();ParsedFeedback=parsed.feedback_pass;
    for(size_t i=0;i<parsed.passes.size();++i)if(parsed.passes[i].float_framebuffer)++ParsedFloatPasses;
    if(ParsedPasses!=14||ParsedFeedback!=6||ParsedFloatPasses==0) {
        FailRealCrt("The selected fixture is not the expected 14-pass Real CRT base preset with feedback pass 6.");
        WriteRealCrtProof();return;
    }
    SetForegroundWindow(HWnd);BringWindowToTop(HWnd);
    Renderer_LoadPreset(NULL,NULL);Renderer_Submit565(TestPattern,320,320,240,0);Renderer_Repaint(PreviewWindow);
    unsigned baselineColors=0;
    if(!ReadFrontBuffer(BaselineHash,baselineColors)){WriteRealCrtProof();return;}
    WriteRealCrtProof();
    RealCrtLoaded=Renderer_LoadPreset(RealCrtPath.c_str(),NULL)!=0;
    if(!RealCrtLoaded){FailRealCrt("Production Renderer_LoadPreset rejected Real CRT; see Gens-KO-renderer.log.");WriteRealCrtProof();return;}
    Renderer_Submit565(TestPattern,320,320,240,0);Renderer_Repaint(PreviewWindow);VerifyRealCrtFrame();
    SetWindowTextW(RendererLabel,Renderer_Status());
}

void CreatePattern() {
    const unsigned short bars[8]={0xffff,0xffe0,0x07ff,0x07e0,0xf81f,0xf800,0x001f,0};
    for(int y=0;y<240;y++)for(int x=0;x<320;x++) {
        unsigned short pixel;
        if(y<80)pixel=bars[x/40];
        else if(y<160) {
            unsigned shade=(unsigned)x*255/319;
            pixel=(unsigned short)(((shade>>3)<<11)|((shade>>2)<<5)|(shade>>3));
        } else {
            int cell=x<160?1:8;
            pixel=((x/cell+y/cell)&1)?0xd69a:0x2104;
        }
        if(x==0||y==0||x==319||y==239)pixel=0xffff;
        TestPattern[y*320+x]=pixel;
    }
}
LRESULT CALLBACK PreviewProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT) {
        PAINTSTRUCT paint;BeginPaint(window,&paint);Renderer_Repaint(window);EndPaint(window,&paint);return 0;
    }
    if(message==WM_SIZE) {Renderer_Resize(window);return 0;}
    return DefWindowProcW(window,message,wp,lp);
}

void ConfigFile() {
    wchar_t path[1024];DWORD count=GetModuleFileNameW(NULL,path,1024);
    if(!count||count>=1000)return;
    wchar_t*slash=wcsrchr(path,L'\\');
    if(!slash)return;
    ExeDirectory.assign(path,(size_t)(slash-path)+1);
    ProofPath=ExeDirectory+L"real-crt-probe.json";
    RealCrtPath=ExeDirectory+L"shaders\\real-crt-phosphor\\real-crt-phosphor.glslp";
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
    case WM_CREATE: {
        BodyFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        Add(window,L"STATIC",L"Gens KO 실제 설정 창 · 셰이더 출력",SS_LEFT,0,28,18,700,30);
        Add(window,L"STATIC",L"제품과 동일한 설정·입력·화면 출력 소스를 실행합니다.\r\n"
            L"아래 영상은 직접 만든 검증 패턴이며 게임 실행 화면이 아닙니다.",SS_LEFT,
            0,28,57,700,48);
        Add(window,L"BUTTON",L"키보드 · 게임패드 설정",WS_TABSTOP|BS_PUSHBUTTON,InputCommand,28,118,325,40);
        Add(window,L"BUTTON",L"핫키 · 재생 속도 설정",WS_TABSTOP|BS_PUSHBUTTON,HotkeyCommand,375,118,325,40);
        Add(window,L"BUTTON",L"화면 출력 · 셰이더 설정",WS_TABSTOP|BS_PUSHBUTTON,ShaderCommand,28,168,672,40);
        WNDCLASSW preview={};preview.style=CS_OWNDC;preview.lpfnWndProc=PreviewProc;
        preview.hInstance=ghInstance;preview.lpszClassName=L"GensKoShaderPreview";
        RegisterClassW(&preview);
        PreviewWindow=CreateWindowExW(0,preview.lpszClassName,L"직접 만든 RGB565 검증 패턴",
            WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,28,226,432,324,window,NULL,ghInstance,NULL);
        Add(window,L"STATIC",L"현재 출력 방식",SS_LEFT,0,480,232,220,30);
        RendererLabel=Add(window,L"STATIC",L"초기화 중",SS_LEFT,0,480,268,220,110);
        Add(window,L"STATIC",L"색 막대 · 명암 단계\r\n1픽셀 / 8픽셀 격자\r\n\r\n"
            L"GPU에서 GLSL을 사용할 수 없으면 실제 GDI 호환 출력으로 전환합니다.",SS_LEFT,
            0,480,390,220,155);
        Add(window,L"STATIC",L"Gens GPL v2 입력 프런트엔드 + Gens KO GPL v2 설정·렌더러.\r\n"
            L"게임 코어·ROM 없음. 보증 없이 제공되며 라이선스와 해당 소스를 함께 배포합니다.",SS_LEFT,
            0,28,580,700,52);
        CreatePattern();Renderer_Init(PreviewWindow);Renderer_SetVSync(0);
        Renderer_Submit565(TestPattern,320,320,240,0);Renderer_Repaint(PreviewWindow);
        SetWindowTextW(RendererLabel,Renderer_Status());SetTimer(window,1,20,NULL);return 0;
    }
    case WM_TIMER:
        Hotkeys_Poll();
        if(PreviewWindow) {
            Renderer_Submit565(TestPattern,320,320,240,0);Renderer_Repaint(PreviewWindow);
            VerifyRealCrtFrame();
            SetWindowTextW(RendererLabel,Renderer_Status());
        }
        return 0;
    case RealCrtLoadMessage:LoadRealCrt();return 0;
    case WM_MOVE:case WM_DISPLAYCHANGE:
        if(PreviewWindow)Renderer_Resize(PreviewWindow);
        return 0;
    case WM_COMMAND:
        if(LOWORD(wParam)==InputCommand) {
            Hotkeys_ClearActive();End_Input();ModernInput_ShowDialog(window);
            Init_Input(ghInstance,window);return 0;
        }
        if(LOWORD(wParam)==HotkeyCommand) {
            Hotkeys_ShowDialog(window);return 0;
        }
        if(LOWORD(wParam)==ShaderCommand) {
            Renderer_ShowSettings(window);
            SetWindowTextW(RendererLabel,Renderer_Status());Renderer_Repaint(PreviewWindow);return 0;
        }
        break;
    case WM_ACTIVATEAPP:
        if(!wParam) {Hotkeys_ClearActive();Input_ClearState();}
        return 0;
    case WM_CLOSE:
        if(RealCrtRequested) {
            if(VerifiedPresentations<3&&!RealCrtFailed)FailRealCrt("Probe closed before three verified GLSL presentations.");
            ClosedNormally=true;WriteRealCrtProof();
        }
        DestroyWindow(window);return 0;
    case WM_DESTROY:
        KillTimer(window,1);ModernInput_SaveConfig(ConfigPath);Hotkeys_SaveConfig(ConfigPath);
        End_Input();Renderer_Shutdown();if(BodyFont)DeleteObject(BodyFont);
        PostQuitMessage(RealCrtRequested&&RealCrtFailed?4:0);return 0;
    }
    return DefWindowProcW(window,message,wParam,lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int show) {
    ghInstance=instance;
    RealCrtRequested=command&&wcsstr(command,L"--real-crt");
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
        50,30,760,690,NULL,NULL,instance,NULL);
    if(!HWnd)return 3;
    ShowWindow(HWnd,show);UpdateWindow(HWnd);Init_Input(instance,HWnd);
    if(RealCrtRequested){WriteRealCrtProof();PostMessageW(HWnd,RealCrtLoadMessage,0,0);}
    if(command&&wcsstr(command,L"--input"))PostMessageW(HWnd,WM_COMMAND,InputCommand,0);
    if(command&&wcsstr(command,L"--transport"))PostMessageW(HWnd,WM_COMMAND,HotkeyCommand,0);
    if(command&&wcsstr(command,L"--shader"))PostMessageW(HWnd,WM_COMMAND,ShaderCommand,0);
    MSG message={};
    while(GetMessageW(&message,NULL,0,0)>0) {
        if(Hotkeys_FilterMessage(&message))continue;
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
