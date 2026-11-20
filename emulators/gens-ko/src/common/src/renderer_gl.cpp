/* Gens KO window renderer and GLSL adapter. Created 2026-11-20.
   GPL-2.0, see LICENSE.txt. Implemented independently from public APIs/specs.
   The game framebuffer is retained on the CPU. Focus/paint/resize never changes
   the display mode and never clears the visible window before replacement. */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "renderer_gl.h"
#include "shader_preset.h"
#include <GL/gl.h>
#include <commdlg.h>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#define GL_RGBA8 0x8058
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_CLAMP_TO_BORDER 0x812D
#define GL_MIRRORED_REPEAT 0x8370
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_SAMPLER_2D 0x8B5E
#define GL_TEXTURE0 0x84C0
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#endif

typedef GLuint (APIENTRY *FnCreateShader)(GLenum);
typedef void (APIENTRY *FnShaderSource)(GLuint, GLsizei, const char **, const GLint *);
typedef void (APIENTRY *FnCompileShader)(GLuint);
typedef void (APIENTRY *FnGetShaderiv)(GLuint,GLenum,GLint *);
typedef void (APIENTRY *FnGetShaderInfoLog)(GLuint,GLsizei,GLsizei *,char *);
typedef void (APIENTRY *FnDeleteShader)(GLuint);
typedef GLuint (APIENTRY *FnCreateProgram)(void);
typedef void (APIENTRY *FnAttachShader)(GLuint,GLuint);
typedef void (APIENTRY *FnLinkProgram)(GLuint);
typedef void (APIENTRY *FnGetProgramiv)(GLuint,GLenum,GLint *);
typedef void (APIENTRY *FnGetProgramInfoLog)(GLuint,GLsizei,GLsizei *,char *);
typedef void (APIENTRY *FnDeleteProgram)(GLuint);
typedef void (APIENTRY *FnUseProgram)(GLuint);
typedef GLint (APIENTRY *FnGetUniformLocation)(GLuint,const char *);
typedef void (APIENTRY *FnUniform1i)(GLint,GLint);
typedef void (APIENTRY *FnUniform1f)(GLint,GLfloat);
typedef void (APIENTRY *FnUniform2f)(GLint,GLfloat,GLfloat);
typedef void (APIENTRY *FnUniform4f)(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
typedef void (APIENTRY *FnUniformMatrix4fv)(GLint,GLsizei,GLboolean,const GLfloat *);
typedef GLint (APIENTRY *FnGetAttribLocation)(GLuint,const char *);
typedef void (APIENTRY *FnEnableVertexAttribArray)(GLuint);
typedef void (APIENTRY *FnDisableVertexAttribArray)(GLuint);
typedef void (APIENTRY *FnVertexAttribPointer)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void *);
typedef void (APIENTRY *FnVertexAttrib4f)(GLuint,GLfloat,GLfloat,GLfloat,GLfloat);
typedef void (APIENTRY *FnActiveTexture)(GLenum);
typedef void (APIENTRY *FnGenFramebuffers)(GLsizei,GLuint *);
typedef void (APIENTRY *FnDeleteFramebuffers)(GLsizei,const GLuint *);
typedef void (APIENTRY *FnBindFramebuffer)(GLenum,GLuint);
typedef void (APIENTRY *FnFramebufferTexture2D)(GLenum,GLenum,GLenum,GLuint,GLint);
typedef GLenum (APIENTRY *FnCheckFramebufferStatus)(GLenum);
typedef void (APIENTRY *FnGetActiveUniform)(GLuint,GLuint,GLsizei,GLsizei *,GLint *,GLenum *,char *);
typedef BOOL (WINAPI *FnSwapInterval)(int);

struct GlFunctions {
    FnCreateShader CreateShader; FnShaderSource ShaderSource; FnCompileShader CompileShader;
    FnGetShaderiv GetShaderiv; FnGetShaderInfoLog GetShaderInfoLog; FnDeleteShader DeleteShader;
    FnCreateProgram CreateProgram; FnAttachShader AttachShader; FnLinkProgram LinkProgram;
    FnGetProgramiv GetProgramiv; FnGetProgramInfoLog GetProgramInfoLog; FnDeleteProgram DeleteProgram;
    FnUseProgram UseProgram; FnGetUniformLocation GetUniformLocation;
    FnUniform1i Uniform1i; FnUniform1f Uniform1f; FnUniform2f Uniform2f; FnUniform4f Uniform4f;
    FnUniformMatrix4fv UniformMatrix4fv; FnGetAttribLocation GetAttribLocation;
    FnEnableVertexAttribArray EnableVertexAttribArray; FnDisableVertexAttribArray DisableVertexAttribArray;
    FnVertexAttribPointer VertexAttribPointer; FnVertexAttrib4f VertexAttrib4f;
    FnActiveTexture ActiveTexture; FnGenFramebuffers GenFramebuffers;
    FnDeleteFramebuffers DeleteFramebuffers; FnBindFramebuffer BindFramebuffer;
    FnFramebufferTexture2D FramebufferTexture2D; FnCheckFramebufferStatus CheckFramebufferStatus;
    FnGetActiveUniform GetActiveUniform; FnSwapInterval SwapInterval;
};
static GlFunctions glx;
struct Pass {
    KoShaderPass spec;
    GLuint program,texture,fbo;
    int width,height;
    Pass():program(0),texture(0),fbo(0),width(0),height(0){}
};
static HWND window=0;
/* A separate child DC makes GDI fallback valid even after a non-GDI-capable
   double-buffered pixel format was set on the parent for OpenGL. */
static HWND gdiSurface=0;
static HDC dc=0;
static HGLRC context=0;
static GLuint sourceTexture=0,stockProgram=0;
static int texWidth=0,texHeight=0,frameWidth=320,frameHeight=240;
static int maxTexture=4096,maxTextureUnits=8;
static unsigned int frameNumber=0;
static bool frameDirty=true,loadedSettings=false,forceGdi=false,keepAspect=true,integerScale=false;
static int vsync=1,lastVsync=-1;
static std::vector<unsigned int> frame(320*240,0xff000000u);
static std::vector<Pass> passes;
static std::map<std::string,float> parameters;
static std::wstring presetPath,baseDir,iniPath,status=L"초기화 중";
static HMONITOR lastMonitor=0;
static std::wstring overlayText;
static DWORD overlayUntil=0;
static bool overlayWasVisible=false;
static std::vector<unsigned int> overlayFrame;
static void gdiDraw();
static LRESULT CALLBACK gdiSurfaceProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT p;BeginPaint(hwnd,&p);gdiDraw();EndPaint(hwnd,&p);return 0;}
    if(msg==WM_LBUTTONDOWN){if(window)SetFocus(window);}
    if(msg==WM_RBUTTONDOWN||msg==WM_MOUSEWHEEL||msg==WM_DROPFILES)
        return window?SendMessageW(window,msg,wp,lp):0;
    return DefWindowProcW(hwnd,msg,wp,lp);
}
static void gdiChild() {
    if(!window)return;
    RECT r;GetClientRect(window,&r);
    if(!gdiSurface){
        WNDCLASSW cls;ZeroMemory(&cls,sizeof(cls));cls.style=CS_OWNDC;cls.lpfnWndProc=gdiSurfaceProc;
        cls.hInstance=GetModuleHandleW(0);cls.lpszClassName=L"GensKoGdiSurface";
        cls.hCursor=LoadCursor(0,IDC_ARROW);RegisterClassW(&cls);
        gdiSurface=CreateWindowExW(0,cls.lpszClassName,L"",WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS,
            0,0,r.right,r.bottom,window,0,cls.hInstance,0);
    } else {
        RECT old;GetClientRect(gdiSurface,&old);
        if(old.right!=r.right||old.bottom!=r.bottom)
            SetWindowPos(gdiSurface,0,0,0,r.right,r.bottom,SWP_NOZORDER|SWP_NOACTIVATE);
    }
}

static std::wstring wide(const std::string &s) {
    if(s.empty())return std::wstring();
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),(int)s.size(),0,0);
    UINT cp=CP_UTF8;DWORD flags=MB_ERR_INVALID_CHARS;
    if(!n){cp=CP_ACP;flags=0;n=MultiByteToWideChar(cp,flags,s.c_str(),(int)s.size(),0,0);}
    std::wstring out(n,L'\0');if(n)MultiByteToWideChar(cp,flags,s.c_str(),(int)s.size(),&out[0],n);return out;
}
static std::string utf8(const std::wstring &s) {
    if(s.empty())return std::string();
    int n=WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),0,0,0,0);
    std::string out(n,'\0');if(n)WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),&out[0],n,0,0);return out;
}
static std::wstring parentPath(const std::wstring &s) {
    size_t p=s.find_last_of(L"/\\");return p==std::wstring::npos?L"":s.substr(0,p+1);
}
static std::wstring joined(const std::wstring &base,const std::string &file) {
    std::wstring path=wide(file);
    if(path.size()>1&&path[1]==L':')return path;
    if(path.size()>1&&(path[0]==L'\\'||path[0]==L'/'))return path;
    return base+path;
}
static std::wstring extension(std::wstring path) {
    size_t p=path.find_last_of(L'.');if(p==std::wstring::npos)return L"";
    path=path.substr(p);for(size_t i=0;i<path.size();++i)if(path[i]>=L'A'&&path[i]<=L'Z')path[i]+=32;return path;
}
static std::string trim(std::string s) {
    size_t a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos?std::string():s.substr(a,b-a+1);
}
static void paths() {
    if(!baseDir.empty())return;
    wchar_t module[32768]={0};GetModuleFileNameW(0,module,32767);
    baseDir=parentPath(module);iniPath=baseDir+L"Gens-KO-video.ini";
}
static void logLine(const std::string &text) {
    paths();FILE *f=_wfopen((baseDir+L"Gens-KO-renderer.log").c_str(),L"ab");
    if(f){fprintf(f,"%s\r\n",text.c_str());fclose(f);}
}
static bool readFile(const std::wstring &path,std::string &out,std::string &error) {
    FILE *f=_wfopen(path.c_str(),L"rb");
    if(!f){error="Cannot read shader file: "+utf8(path);return false;}
    fseek(f,0,SEEK_END);long len=ftell(f);rewind(f);
    if(len<0||len>2*1024*1024){fclose(f);error="Shader file exceeds 2 MiB.";return false;}
    out.assign((size_t)len,'\0');size_t n=len?fread(&out[0],1,len,f):0;fclose(f);
    if(n!=(size_t)len){error="Incomplete shader file read.";return false;}
    if(out.compare(0,3,"\xef\xbb\xbf")==0)out.erase(0,3);
    if(out.find('\0')!=std::string::npos){error="Shader must be a UTF-8 text file.";return false;}
    return true;
}
static bool shaderText(const std::wstring &path,std::string &out,std::string &error,int depth=0) {
    if(depth>8){error="Shader #include recursion exceeds 8 levels.";return false;}
    std::string raw;if(!readFile(path,raw,error))return false;
    std::istringstream lines(raw);std::string line;out.clear();
    while(std::getline(lines,line)) {
        std::string t=trim(line);
        if(t.compare(0,8,"#include")==0) {
            size_t a=t.find('"'),b=t.find('"',a==std::string::npos?0:a+1);
            if(a==std::string::npos||b==std::string::npos){error="Use relative quoted #include paths.";return false;}
            std::string nested;if(!shaderText(joined(parentPath(path),t.substr(a+1,b-a-1)),nested,error,depth+1))return false;
            out+=nested+"\n";
        } else out+=line+"\n";
        if(out.size()>4*1024*1024){error="Expanded shader exceeds 4 MiB.";return false;}
    }
    return true;
}
static void loadSettings() {
    if(loadedSettings)return;loadedSettings=true;paths();
    keepAspect=GetPrivateProfileIntW(L"Video",L"KeepAspect",1,iniPath.c_str())!=0;
    integerScale=GetPrivateProfileIntW(L"Video",L"IntegerScale",0,iniPath.c_str())!=0;
    forceGdi=GetPrivateProfileIntW(L"Video",L"GDI",0,iniPath.c_str())!=0;
    wchar_t path[32768]={0};GetPrivateProfileStringW(L"Video",L"Shader",L"",path,32767,iniPath.c_str());
    presetPath=path;
}
static bool saveSettings() {
    return WritePrivateProfileStringW(L"Video",L"KeepAspect",keepAspect?L"1":L"0",iniPath.c_str())&&
       WritePrivateProfileStringW(L"Video",L"IntegerScale",integerScale?L"1":L"0",iniPath.c_str())&&
       WritePrivateProfileStringW(L"Video",L"GDI",forceGdi?L"1":L"0",iniPath.c_str())&&
       WritePrivateProfileStringW(L"Video",L"Shader",presetPath.c_str(),iniPath.c_str());
}
static PROC proc(const char *name,const char *alternate=0) {
    PROC p=wglGetProcAddress(name);
    if(!p||p==(PROC)1||p==(PROC)2||p==(PROC)3||p==(PROC)-1) {
        p=alternate?wglGetProcAddress(alternate):0;
        if(!p||p==(PROC)1||p==(PROC)2||p==(PROC)3||p==(PROC)-1)p=0;
    }
    return p;
}
static bool loadGl() {
#define LOAD(name) glx.name=(Fn##name)proc("gl" #name);if(!glx.name)return false
    LOAD(CreateShader);LOAD(ShaderSource);LOAD(CompileShader);LOAD(GetShaderiv);LOAD(GetShaderInfoLog);LOAD(DeleteShader);
    LOAD(CreateProgram);LOAD(AttachShader);LOAD(LinkProgram);LOAD(GetProgramiv);LOAD(GetProgramInfoLog);LOAD(DeleteProgram);
    LOAD(UseProgram);LOAD(GetUniformLocation);LOAD(Uniform1i);LOAD(Uniform1f);LOAD(Uniform2f);LOAD(Uniform4f);LOAD(UniformMatrix4fv);
    LOAD(GetAttribLocation);LOAD(EnableVertexAttribArray);LOAD(DisableVertexAttribArray);LOAD(VertexAttribPointer);LOAD(VertexAttrib4f);
    LOAD(ActiveTexture);LOAD(GetActiveUniform);
#undef LOAD
#define LOADEXT(name) glx.name=(Fn##name)proc("gl" #name,"gl" #name "EXT");if(!glx.name)return false
    LOADEXT(GenFramebuffers);LOADEXT(DeleteFramebuffers);LOADEXT(BindFramebuffer);LOADEXT(FramebufferTexture2D);LOADEXT(CheckFramebufferStatus);
#undef LOADEXT
    glx.SwapInterval=(FnSwapInterval)proc("wglSwapIntervalEXT");
    glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxTexture);maxTexture=std::min(maxTexture,8192);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&maxTextureUnits);
    return maxTexture>=1024&&maxTextureUnits>=8;
}
static std::string stageSource(const std::string &raw,bool vertex) {
    std::istringstream lines(raw);std::string line,body,version="#version 120\n";
    while(std::getline(lines,line)) {
        std::string t=trim(line);
        if(t.compare(0,8,"#version")==0)version=t+"\n";
        else if(t.compare(0,17,"#pragma parameter")==0)body+="\n";
        else body+=line+"\n";
    }
    return version+(vertex?"#define VERTEX 1\n":"#define FRAGMENT 1\n")+
        "#define PARAMETER_UNIFORM 1\n"+body;
}
static GLuint compileStage(const std::string &source,GLenum type,std::string &error) {
    GLuint id=glx.CreateShader(type);const char *p=source.c_str();
    glx.ShaderSource(id,1,&p,0);glx.CompileShader(id);GLint ok=0;glx.GetShaderiv(id,GL_COMPILE_STATUS,&ok);
    if(!ok) {char msg[8192]={0};glx.GetShaderInfoLog(id,sizeof(msg)-1,0,msg);error=msg;glx.DeleteShader(id);return 0;}
    return id;
}
static GLuint compileProgram(const std::string &raw,std::string &error) {
    GLuint v=compileStage(stageSource(raw,true),GL_VERTEX_SHADER,error);if(!v)return 0;
    GLuint f=compileStage(stageSource(raw,false),GL_FRAGMENT_SHADER,error);if(!f){glx.DeleteShader(v);return 0;}
    GLuint p=glx.CreateProgram();glx.AttachShader(p,v);glx.AttachShader(p,f);glx.LinkProgram(p);
    glx.DeleteShader(v);glx.DeleteShader(f);GLint ok=0;glx.GetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char msg[8192]={0};glx.GetProgramInfoLog(p,sizeof(msg)-1,0,msg);error=msg;glx.DeleteProgram(p);return 0;}
    return p;
}
static const char stockSource[]=
    "#if defined(VERTEX)\nattribute vec4 VertexCoord; attribute vec2 TexCoord;\n"
    "uniform mat4 MVPMatrix; varying vec2 coord;\n"
    "void main(){gl_Position=MVPMatrix*VertexCoord;coord=TexCoord;}\n"
    "#elif defined(FRAGMENT)\nuniform sampler2D Texture; varying vec2 coord;\n"
    "void main(){gl_FragColor=texture2D(Texture,coord);}\n#endif\n";

static void destroyPasses(std::vector<Pass> &v) {
    for(size_t i=0;i<v.size();++i){
        if(v[i].program)glx.DeleteProgram(v[i].program);
        if(v[i].texture)glDeleteTextures(1,&v[i].texture);
        if(v[i].fbo)glx.DeleteFramebuffers(1,&v[i].fbo);
    }
    v.clear();
}
static void closeGl() {
    if(context){
        if(wglMakeCurrent(dc,context)) {
            if(glx.DeleteProgram){destroyPasses(passes);if(stockProgram)glx.DeleteProgram(stockProgram);}
            if(sourceTexture)glDeleteTextures(1,&sourceTexture);
        } else passes.clear();
        wglMakeCurrent(0,0);wglDeleteContext(context);context=0;
    }
    stockProgram=sourceTexture=0;texWidth=texHeight=0;lastVsync=-1;
    ZeroMemory(&glx,sizeof(glx));
}
static void errorBox(HWND parent,const std::string &message) {
    logLine(message);if(!parent)return;
    std::wstring text=L"셰이더를 적용하지 못했습니다. 기존 화면 설정을 유지합니다.\n\n"+wide(message);
    MessageBoxW(parent,text.c_str(),L"Gens KO 셰이더",MB_OK|MB_ICONWARNING);
}
static void parameterDefaults(const std::string &source,std::map<std::string,float> &values) {
    std::istringstream lines(source);std::string line;
    while(std::getline(lines,line)) {
        line=trim(line);if(line.compare(0,17,"#pragma parameter")!=0)continue;
        std::istringstream nameTokens(line.substr(17));std::string name;nameTokens>>name;
        size_t first=line.find('"'),last=line.find('"',first==std::string::npos?0:first+1);
        if(name.empty()||first==std::string::npos||last==std::string::npos)continue;
        float value=0,minimum=0,maximum=0;std::istringstream numbers(line.substr(last+1));
        if(numbers>>value>>minimum>>maximum) {
            if(!values.count(name))values[name]=value;
            values[name]=std::max(minimum,std::min(maximum,values[name]));
        }
    }
}
static bool supportedUniforms(GLuint program,const KoShaderPreset &preset,unsigned int pass,std::string &error) {
    GLint count=0;glx.GetProgramiv(program,GL_ACTIVE_UNIFORMS,&count);
    for(GLint i=0;i<count;++i) {
        char name[256]={0};GLint size=0;GLenum type=0;
        glx.GetActiveUniform(program,(GLuint)i,sizeof(name)-1,0,&size,&type,name);
        std::string n(name);
        if(n.compare(0,4,"Prev")==0||n.find("Feedback")!=std::string::npos) {
            error="Previous-frame/feedback shaders are not supported: "+n;return false;
        }
        if(type==GL_SAMPLER_2D) {
            bool ok=n=="Texture"||n=="Source"||n=="OrigTexture";
            for(unsigned int j=0;j<pass;++j) {
                std::ostringstream key;key<<"Pass"<<j+1<<"Texture";
                if(n==key.str())ok=true;
                std::ostringstream prev;prev<<"PassPrev"<<pass-j<<"Texture";
                if(n==prev.str())ok=true;
                if(!preset.passes[j].alias.empty()&&n==preset.passes[j].alias+"Texture")ok=true;
            }
            if(!ok||size!=1){error="Unsupported texture input: "+n;return false;}
        }
    }
    return true;
}

int Renderer_LoadPreset(const wchar_t *file,HWND errorParent) {
    loadSettings();std::wstring path=file?file:L"";
    if(path.empty()) {
        if(context&&wglMakeCurrent(dc,context))destroyPasses(passes);
        presetPath.clear();parameters.clear();status=context?L"OpenGL · 셰이더 끔":L"GDI 호환 출력 · 셰이더 끔";
        Renderer_Repaint(window);return 1;
    }
    if(extension(path)!=L".glsl"&&extension(path)!=L".glslp") {
        errorBox(errorParent,"지원 형식은 .glsl / .glslp입니다. Slang(.slangp)와 Cg(.cgp)는 지원하지 않습니다.");return 0;
    }
    if(!context||!wglMakeCurrent(dc,context)) {
        errorBox(errorParent,"셰이더에는 OpenGL 2.0 이상이 필요합니다. GDI 호환 출력에서는 사용할 수 없습니다.");return 0;
    }
    KoShaderPreset preset;std::string data,error;
    if(extension(path)==L".glsl") {
        KoShaderPass p;p.file=utf8(path);p.scale_type_x=p.scale_type_y="viewport";preset.passes.push_back(p);
    } else if(!readFile(path,data,error)||!KoParseShaderPreset(data,preset,error)) {errorBox(errorParent,error);return 0;}
    if((int)preset.passes.size()+2>maxTextureUnits) {errorBox(errorParent,"이 GPU의 텍스처 슬롯 수로는 이 프리셋을 실행할 수 없습니다.");return 0;}
    std::vector<Pass> candidate;std::map<std::string,float> values=preset.parameters;
    for(size_t i=0;i<preset.passes.size();++i) {
        Pass p;p.spec=preset.passes[i];std::wstring shader=joined(parentPath(path),p.spec.file);
        if(extension(shader)!=L".glsl"){error="All passes must use .glsl files.";destroyPasses(candidate);errorBox(errorParent,error);return 0;}
        if(!shaderText(shader,data,error)){destroyPasses(candidate);errorBox(errorParent,error);return 0;}
        parameterDefaults(data,values);p.program=compileProgram(data,error);
        if(!p.program||!supportedUniforms(p.program,preset,(unsigned int)i,error)) {
            if(p.program)glx.DeleteProgram(p.program);destroyPasses(candidate);errorBox(errorParent,error);return 0;
        }
        candidate.push_back(p);
    }
    destroyPasses(passes);passes.swap(candidate);parameters.swap(values);presetPath=path;
    std::wostringstream state;state<<L"OpenGL · GLSL "<<passes.size()<<L"패스";status=state.str();
    logLine("Loaded GLSL preset: "+utf8(path));Renderer_Repaint(window);return 1;
}

int Renderer_Init(HWND hwnd) {
    loadSettings();if(context&&window==hwnd)return 1;
    if(window&&window!=hwnd)Renderer_Shutdown();window=hwnd;
    if(!dc)dc=GetDC(window);
    if(!dc){status=L"창 그리기 영역을 확보하지 못했습니다";return 0;}
    lastMonitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);
    if(forceGdi){status=L"GDI 호환 출력 · 셰이더 없음";return 1;}
    if(gdiSurface){DestroyWindow(gdiSurface);gdiSurface=0;}
    PIXELFORMATDESCRIPTOR pfd;ZeroMemory(&pfd,sizeof(pfd));
    pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
    pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=32;pfd.iLayerType=PFD_MAIN_PLANE;
    int format=GetPixelFormat(dc);
    if(!format){format=ChoosePixelFormat(dc,&pfd);if(!format||!SetPixelFormat(dc,format,&pfd))format=0;}
    if(format)context=wglCreateContext(dc);
    if(!context||!wglMakeCurrent(dc,context)||!loadGl()) {
        closeGl();status=L"GDI 호환 출력 · OpenGL을 사용할 수 없음";logLine("OpenGL unavailable; using retained-frame GDI renderer.");return 1;
    }
    std::string error;stockProgram=compileProgram(stockSource,error);
    if(!stockProgram){logLine(error);closeGl();status=L"GDI 호환 출력 · OpenGL 초기화 실패";return 1;}
    const char *vendor=(const char *)glGetString(GL_VENDOR),*version=(const char *)glGetString(GL_VERSION);
    logLine(std::string("OpenGL: ")+(vendor?vendor:"")+" / "+(version?version:""));
    glGenTextures(1,&sourceTexture);frameDirty=true;
    glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);
    glClearColor(0,0,0,1);status=L"OpenGL · 셰이더 끔";
    std::wstring requested=presetPath;
    if(!requested.empty()&&!Renderer_LoadPreset(requested.c_str(),0))status=L"OpenGL · 저장된 셰이더 오류, 기본 출력";
    return 1;
}
void Renderer_Shutdown() {
    if(gdiSurface){DestroyWindow(gdiSurface);gdiSurface=0;}
    closeGl();if(dc&&window)ReleaseDC(window,dc);dc=0;window=0;
}
void Renderer_Submit565(const unsigned short *pixels,int stride,int width,int height,int rgb555) {
    if(!pixels||width<1||height<1||width>1024||height>512||stride<width)return;
    frameWidth=width;frameHeight=height;frame.resize((size_t)width*height);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        unsigned int c=pixels[y*stride+x],r,g,b;
        if(rgb555){r=(c>>10)&31;g=(c>>5)&31;b=c&31;r=(r<<3)|(r>>2);g=(g<<3)|(g>>2);}
        else{r=(c>>11)&31;g=(c>>5)&63;b=c&31;r=(r<<3)|(r>>2);g=(g<<2)|(g>>4);}
        b=(b<<3)|(b>>2);frame[(size_t)y*width+x]=0xff000000u|(r<<16)|(g<<8)|b;
    }
    frameDirty=true;frameNumber=(frameNumber+1)&0x7fffffffu;
}
void Renderer_SetOverlayUtf8(const char *text,int duration_ms) {
    overlayText=text?wide(text):std::wstring();
    if(duration_ms<=0)overlayText.clear();
    overlayUntil=GetTickCount()+(DWORD)std::min(std::max(duration_ms,0),60000);
    frameDirty=true;
}
static const unsigned int *displayPixels() {
    bool visible=!overlayText.empty()&&(LONG)(overlayUntil-GetTickCount())>0;
    if(visible!=overlayWasVisible){frameDirty=true;overlayWasVisible=visible;}
    if(!visible)return &frame[0];
    if(!frameDirty&&overlayFrame.size()==frame.size())return &overlayFrame[0];
    overlayFrame=frame;
    BITMAPINFO bmi;ZeroMemory(&bmi,sizeof(bmi));bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth=frameWidth;bmi.bmiHeader.biHeight=-frameHeight;
    bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;
    HDC memory=CreateCompatibleDC(0);void *bits=0;
    HBITMAP bitmap=CreateDIBSection(memory,&bmi,DIB_RGB_COLORS,&bits,0,0);
    if(memory&&bitmap&&bits){
        memcpy(bits,&frame[0],frame.size()*sizeof(unsigned int));
        HGDIOBJ old=SelectObject(memory,bitmap);int fontHeight=frameWidth>=512?24:13;
        HFONT font=CreateFontW(-fontHeight,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        HGDIOBJ oldFont=SelectObject(memory,font);
        RECT box={4,frameHeight-fontHeight*2-12,frameWidth-4,frameHeight-4};
        FillRect(memory,&box,(HBRUSH)GetStockObject(BLACK_BRUSH));box.left+=5;box.right-=5;box.top+=3;
        SetBkMode(memory,TRANSPARENT);SetTextColor(memory,RGB(255,255,255));
        DrawTextW(memory,overlayText.c_str(),-1,&box,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX|DT_END_ELLIPSIS);
        memcpy(&overlayFrame[0],bits,frame.size()*sizeof(unsigned int));
        SelectObject(memory,oldFont);SelectObject(memory,old);DeleteObject(font);
    }
    if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);
    return &overlayFrame[0];
}
static RECT viewport(int width,int height) {
    RECT r={0,0,width,height};int w=width,h=height;
    if(integerScale&&width>=frameWidth&&height>=frameHeight) {
        int k=std::min(width/frameWidth,height/frameHeight);w=frameWidth*k;h=frameHeight*k;
    } else if(keepAspect) {
        if((double)width/height>4.0/3.0)w=(int)(height*4.0/3.0+0.5);
        else h=(int)(width*3.0/4.0+0.5);
    }
    r.left=(width-w)/2;r.top=(height-h)/2;r.right=r.left+w;r.bottom=r.top+h;return r;
}
static void gdiDraw() {
    RECT client;GetClientRect(window,&client);int w=client.right,h=client.bottom;if(w<=0||h<=0)return;
    gdiChild();HWND targetWindow=gdiSurface?gdiSurface:window;
    HDC target=GetDC(targetWindow);if(!target)return;
    HDC memory=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,w,h);
    if(memory&&bitmap){
        HGDIOBJ old=SelectObject(memory,bitmap);FillRect(memory,&client,(HBRUSH)GetStockObject(BLACK_BRUSH));
        BITMAPINFO bmi;ZeroMemory(&bmi,sizeof(bmi));bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth=frameWidth;bmi.bmiHeader.biHeight=-frameHeight;bmi.bmiHeader.biPlanes=1;
        bmi.bmiHeader.biBitCount=32;bmi.bmiHeader.biCompression=BI_RGB;RECT dest=viewport(w,h);
        SetStretchBltMode(memory,COLORONCOLOR);
        StretchDIBits(memory,dest.left,dest.top,dest.right-dest.left,dest.bottom-dest.top,
            0,0,frameWidth,frameHeight,displayPixels(),&bmi,DIB_RGB_COLORS,SRCCOPY);
        BitBlt(target,0,0,w,h,memory,0,0,SRCCOPY);SelectObject(memory,old);
    }
    if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);ReleaseDC(targetWindow,target);
}
static void uniform1i(GLuint p,const char *name,int v){GLint loc=glx.GetUniformLocation(p,name);if(loc>=0)glx.Uniform1i(loc,v);}
static void uniform2f(GLuint p,const char *name,float x,float y){GLint loc=glx.GetUniformLocation(p,name);if(loc>=0)glx.Uniform2f(loc,x,y);}
static void bindTexture(GLuint program,const std::string &prefix,int unit,GLuint texture,int width,int height) {
    glx.ActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,texture);
    uniform1i(program,(prefix+"Texture").c_str(),unit);
    uniform2f(program,(prefix+"InputSize").c_str(),(float)width,(float)height);
    uniform2f(program,(prefix+"TextureSize").c_str(),(float)width,(float)height);
}
static void textureOptions(bool linear,const std::string &wrap) {
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,linear?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,linear?GL_LINEAR:GL_NEAREST);
    GLenum w=GL_CLAMP_TO_BORDER;
    if(wrap=="clamp_to_edge")w=GL_CLAMP_TO_EDGE;
    else if(wrap=="repeat")w=GL_REPEAT;
    else if(wrap=="mirrored_repeat")w=GL_MIRRORED_REPEAT;
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,w);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,w);
    const GLfloat border[4]={0,0,0,0};glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,border);
}
static void draw(GLuint program,GLuint texture,int inputWidth,int inputHeight,int outputWidth,int outputHeight,
                 bool flip,int passIndex) {
    glx.UseProgram(program);
    static const GLfloat matrix[16]={2,0,0,0,0,2,0,0,0,0,1,0,-1,-1,0,1};
    GLint loc=glx.GetUniformLocation(program,"MVPMatrix");if(loc>=0)glx.UniformMatrix4fv(loc,1,GL_FALSE,matrix);
    uniform1i(program,"Texture",0);uniform1i(program,"Source",0);
    uniform2f(program,"InputSize",(float)inputWidth,(float)inputHeight);
    uniform2f(program,"TextureSize",(float)inputWidth,(float)inputHeight);
    uniform2f(program,"OutputSize",(float)outputWidth,(float)outputHeight);
    uniform1i(program,"FrameCount",passIndex>=0&&passes[passIndex].spec.frame_mod?
        frameNumber%passes[passIndex].spec.frame_mod:frameNumber);
    uniform1i(program,"FrameDirection",1);
    GLint sourceSize=glx.GetUniformLocation(program,"SourceSize");
    if(sourceSize>=0)glx.Uniform4f(sourceSize,(float)inputWidth,(float)inputHeight,1.0f/inputWidth,1.0f/inputHeight);
    bindTexture(program,"Orig",1,sourceTexture,frameWidth,frameHeight);
    for(int j=0;j<passIndex;++j){
        std::ostringstream key;key<<"Pass"<<j+1;bindTexture(program,key.str(),j+2,passes[j].texture,passes[j].width,passes[j].height);
        std::ostringstream prev;prev<<"PassPrev"<<passIndex-j;bindTexture(program,prev.str(),j+2,passes[j].texture,passes[j].width,passes[j].height);
        if(!passes[j].spec.alias.empty())bindTexture(program,passes[j].spec.alias,j+2,passes[j].texture,passes[j].width,passes[j].height);
    }
    for(std::map<std::string,float>::const_iterator it=parameters.begin();it!=parameters.end();++it){
        loc=glx.GetUniformLocation(program,it->first.c_str());if(loc>=0)glx.Uniform1f(loc,it->second);
    }
    glx.ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
    static const GLfloat vertex[]={0,0,0,1,1,0,0,1,0,1,0,1,1,1,0,1};
    static const GLfloat normal[]={0,0,1,0,0,1,1,1},flipped[]={0,1,1,1,0,0,1,0};
    const GLfloat *coords=flip?flipped:normal;std::vector<GLuint> enabled;
    loc=glx.GetAttribLocation(program,"VertexCoord");if(loc>=0){glx.EnableVertexAttribArray(loc);glx.VertexAttribPointer(loc,4,GL_FLOAT,GL_FALSE,0,vertex);enabled.push_back(loc);}
    std::vector<std::string> names;names.push_back("TexCoord");names.push_back("OrigTexCoord");
    for(int j=0;j<passIndex;++j){std::ostringstream a,b;a<<"Pass"<<j+1<<"TexCoord";b<<"PassPrev"<<passIndex-j<<"TexCoord";names.push_back(a.str());names.push_back(b.str());
       if(!passes[j].spec.alias.empty())names.push_back(passes[j].spec.alias+"TexCoord");}
    for(size_t j=0;j<names.size();++j){loc=glx.GetAttribLocation(program,names[j].c_str());if(loc>=0){glx.EnableVertexAttribArray(loc);glx.VertexAttribPointer(loc,2,GL_FLOAT,GL_FALSE,0,coords);enabled.push_back(loc);}}
    loc=glx.GetAttribLocation(program,"Color");if(loc>=0)glx.VertexAttrib4f(loc,1,1,1,1);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    for(size_t j=0;j<enabled.size();++j)glx.DisableVertexAttribArray(enabled[j]);
}
static int scaled(double value,const std::string &type,int input,int view) {
    double n=type=="source"?value*input:(type=="viewport"?value*view:value);
    if(n<1||n>maxTexture||n!=n)return 0;return(int)(n+0.5);
}
static bool passTarget(Pass &p,int w,int h) {
    if(!p.texture)glGenTextures(1,&p.texture);
    if(!p.fbo)glx.GenFramebuffers(1,&p.fbo);
    glx.ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,p.texture);
    if(p.width!=w||p.height!=h){glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_BGRA,GL_UNSIGNED_BYTE,0);p.width=w;p.height=h;}
    textureOptions(false,"clamp_to_border");glx.BindFramebuffer(GL_FRAMEBUFFER,p.fbo);
    glx.FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,p.texture,0);
    return glx.CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
}
static bool glDrawFrame() {
    if(!wglMakeCurrent(dc,context))return false;
    RECT client;GetClientRect(window,&client);if(client.right<=0||client.bottom<=0)return true;
    RECT dest=viewport(client.right,client.bottom);int viewWidth=dest.right-dest.left,viewHeight=dest.bottom-dest.top;
    if(glx.SwapInterval&&lastVsync!=vsync){glx.SwapInterval(vsync?1:0);lastVsync=vsync;}
    const unsigned int *pixels=displayPixels();
    glx.ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,sourceTexture);
    if(texWidth!=frameWidth||texHeight!=frameHeight){
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,frameWidth,frameHeight,0,GL_BGRA,GL_UNSIGNED_BYTE,pixels);
        texWidth=frameWidth;texHeight=frameHeight;frameDirty=false;
    } else if(frameDirty){glTexSubImage2D(GL_TEXTURE_2D,0,0,0,frameWidth,frameHeight,GL_BGRA,GL_UNSIGNED_BYTE,pixels);frameDirty=false;}
    textureOptions(false,"clamp_to_border");
    GLuint input=sourceTexture;int iw=frameWidth,ih=frameHeight;unsigned long long total=0;
    for(size_t i=0;i<passes.size();++i){
        Pass &p=passes[i];int w=scaled(p.spec.scale_x,p.spec.scale_type_x,iw,viewWidth),h=scaled(p.spec.scale_y,p.spec.scale_type_y,ih,viewHeight);
        total+=(unsigned long long)w*h;
        if(!w||!h||total>32u*1024u*1024u||!passTarget(p,w,h)) {
            status=L"OpenGL · 셰이더 출력 크기/메모리 오류, 기본 출력";logLine("Shader framebuffer allocation failed or exceeds 32M pixels.");
            glx.BindFramebuffer(GL_FRAMEBUFFER,0);destroyPasses(passes);input=sourceTexture;iw=frameWidth;ih=frameHeight;break;
        }
        glViewport(0,0,w,h);glClear(GL_COLOR_BUFFER_BIT);glx.ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,input);
        textureOptions(p.spec.linear,p.spec.wrap);draw(p.program,input,iw,ih,w,h,false,(int)i);
        input=p.texture;iw=w;ih=h;
    }
    glx.BindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,client.right,client.bottom);glClear(GL_COLOR_BUFFER_BIT);
    glViewport(dest.left,client.bottom-dest.bottom,viewWidth,viewHeight);
    glx.ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,input);textureOptions(false,"clamp_to_edge");
    draw(stockProgram,input,iw,ih,viewWidth,viewHeight,true,-1);glx.UseProgram(0);
    return SwapBuffers(dc)!=FALSE;
}
void Renderer_Repaint(HWND hwnd) {
    if(!hwnd||IsIconic(hwnd))return;
    if(!window&&!Renderer_Init(hwnd))return;
    if(hwnd!=window)return;
    if(context){
        if(glDrawFrame())return;
        logLine("OpenGL presentation failed; falling back to retained-frame GDI.");closeGl();status=L"GDI 호환 출력 · 화면 장치 변경 후 복구";
    }
    gdiDraw();
}
void Renderer_Resize(HWND hwnd) {
    if(hwnd==window){
        HMONITOR monitor=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
        if(lastMonitor&&monitor!=lastMonitor){Renderer_DisplayChanged(hwnd);return;}
        Renderer_Repaint(hwnd);
    }
}
void Renderer_DisplayChanged(HWND hwnd) {
    if(!hwnd)return;
    std::wstring saved=presetPath;Renderer_Shutdown();presetPath=saved;Renderer_Init(hwnd);Renderer_Repaint(hwnd);
}
void Renderer_SetVSync(int enabled){vsync=enabled?1:0;}
const wchar_t *Renderer_Status(){return status.c_str();}

enum { VIDEO_PRESET=5101,VIDEO_BROWSE,VIDEO_PATH,VIDEO_ASPECT,VIDEO_INTEGER,VIDEO_GDI,VIDEO_APPLY,VIDEO_STATE,VIDEO_CLOSE };
static HFONT dialogFont=0;
static HWND control(HWND owner,const wchar_t *type,const wchar_t *text,DWORD style,int x,int y,int w,int h,int id) {
    HWND child=CreateWindowExW(type==std::wstring(L"EDIT")?WS_EX_CLIENTEDGE:0,type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,owner,(HMENU)(INT_PTR)id,GetModuleHandleW(0),0);
    SendMessageW(child,WM_SETFONT,(WPARAM)dialogFont,TRUE);return child;
}
static void selectBuiltin(HWND dlg) {
    int s=(int)SendDlgItemMessageW(dlg,VIDEO_PRESET,CB_GETCURSEL,0,0);
    const wchar_t *files[]={L"",L"shaders\\sharp.glslp",L"shaders\\smooth.glslp",L"shaders\\scanlines.glslp",L"shaders\\crt.glslp",L"shaders\\two-pass.glslp",L"shaders\\retroarch-crt-pi.glslp"};
    if(s>=0&&s<7)SetDlgItemTextW(dlg,VIDEO_PATH,s==0?L"":(baseDir+files[s]).c_str());
}
static LRESULT CALLBACK videoProc(HWND dlg,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_CREATE){
        dialogFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        control(dlg,L"STATIC",L"화면 출력 / RetroArch GLSL 셰이더",0,20,16,600,26,0);
        HWND combo=control(dlg,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,20,54,320,220,VIDEO_PRESET);
        const wchar_t *names[]={L"셰이더 끔",L"선명한 픽셀",L"부드러운 화면",L"가벼운 스캔라인",L"CRT 모니터",L"2패스 데모",L"RetroArch CRT-Pi",L"외부 GLSL 프리셋"};
        for(int i=0;i<8;++i)SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)names[i]);
        SendMessageW(combo,CB_SETCURSEL,presetPath.empty()?0:7,0);
        control(dlg,L"BUTTON",L"파일 선택...",WS_TABSTOP|BS_PUSHBUTTON,350,53,130,30,VIDEO_BROWSE);
        control(dlg,L"EDIT",presetPath.c_str(),WS_TABSTOP|ES_AUTOHSCROLL,20,96,640,28,VIDEO_PATH);
        HWND c=control(dlg,L"BUTTON",L"4:3 화면 비율 유지",WS_TABSTOP|BS_AUTOCHECKBOX,20,145,290,28,VIDEO_ASPECT);
        SendMessageW(c,BM_SETCHECK,keepAspect?BST_CHECKED:BST_UNCHECKED,0);
        c=control(dlg,L"BUTTON",L"정수 배율 (정사각 픽셀)",WS_TABSTOP|BS_AUTOCHECKBOX,330,145,330,28,VIDEO_INTEGER);
        SendMessageW(c,BM_SETCHECK,integerScale?BST_CHECKED:BST_UNCHECKED,0);
        c=control(dlg,L"BUTTON",L"GDI 호환 출력 사용 (셰이더 제외)",WS_TABSTOP|BS_AUTOCHECKBOX,20,184,620,28,VIDEO_GDI);
        SendMessageW(c,BM_SETCHECK,forceGdi?BST_CHECKED:BST_UNCHECKED,0);
        control(dlg,L"STATIC",L".glsl / .glslp · 최대 8패스 · 배율/필터/기본 파라미터 지원\nSlang/Cg, LUT, 이전 프레임, 피드백, float/sRGB 버퍼는 지원하지 않습니다.\n모니터 이동과 포커스 변경 시 마지막 화면을 유지합니다.",0,20,230,640,83,0);
        control(dlg,L"STATIC",status.c_str(),0,20,322,640,42,VIDEO_STATE);
        control(dlg,L"BUTTON",L"적용",WS_TABSTOP|BS_DEFPUSHBUTTON,440,382,105,34,VIDEO_APPLY);
        control(dlg,L"BUTTON",L"닫기",WS_TABSTOP|BS_PUSHBUTTON,555,382,105,34,VIDEO_CLOSE);
        return 0;
    }
    if(msg==WM_COMMAND){
        int id=LOWORD(wp);
        if(id==VIDEO_PRESET&&HIWORD(wp)==CBN_SELCHANGE){selectBuiltin(dlg);return 0;}
        if(id==VIDEO_BROWSE){
            wchar_t file[32768]={0};GetDlgItemTextW(dlg,VIDEO_PATH,file,32767);
            OPENFILENAMEW ofn;ZeroMemory(&ofn,sizeof(ofn));ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=dlg;
            ofn.lpstrFilter=L"GLSL 셰이더/프리셋 (*.glsl;*.glslp)\0*.glsl;*.glslp\0모든 파일\0*.*\0\0";
            ofn.lpstrFile=file;ofn.nMaxFile=32767;ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
            if(GetOpenFileNameW(&ofn)){SetDlgItemTextW(dlg,VIDEO_PATH,file);SendDlgItemMessageW(dlg,VIDEO_PRESET,CB_SETCURSEL,7,0);}return 0;
        }
        if(id==VIDEO_APPLY){
            bool gdi=SendDlgItemMessageW(dlg,VIDEO_GDI,BM_GETCHECK,0,0)==BST_CHECKED;
            wchar_t file[32768]={0};GetDlgItemTextW(dlg,VIDEO_PATH,file,32767);
            bool previousGdi=forceGdi;
            if(gdi!=forceGdi){HWND target=window;forceGdi=gdi;Renderer_Shutdown();Renderer_Init(target);}
            if(!gdi&&!Renderer_LoadPreset(file,dlg)){
                if(forceGdi!=previousGdi){HWND target=window;forceGdi=previousGdi;Renderer_Shutdown();Renderer_Init(target);}
                SendDlgItemMessageW(dlg,VIDEO_GDI,BM_SETCHECK,forceGdi?BST_CHECKED:BST_UNCHECKED,0);
                SetDlgItemTextW(dlg,VIDEO_STATE,status.c_str());return 0;
            }
            if(gdi)presetPath=file;
            keepAspect=SendDlgItemMessageW(dlg,VIDEO_ASPECT,BM_GETCHECK,0,0)==BST_CHECKED;
            integerScale=SendDlgItemMessageW(dlg,VIDEO_INTEGER,BM_GETCHECK,0,0)==BST_CHECKED;
            if(!saveSettings())MessageBoxW(dlg,L"설정 파일을 저장할 수 없습니다. 쓰기 가능한 폴더에서 실행해 주세요.",L"Gens KO",MB_OK|MB_ICONWARNING);
            SetDlgItemTextW(dlg,VIDEO_STATE,status.c_str());Renderer_Repaint(window);return 0;
        }
        if(id==VIDEO_CLOSE||id==IDCANCEL){DestroyWindow(dlg);return 0;}
    }
    if(msg==WM_CLOSE){DestroyWindow(dlg);return 0;}
    if(msg==WM_DESTROY){if(dialogFont){DeleteObject(dialogFont);dialogFont=0;}return 0;}
    return DefWindowProcW(dlg,msg,wp,lp);
}
void Renderer_ShowSettings(HWND parent) {
    loadSettings();WNDCLASSW cls;ZeroMemory(&cls,sizeof(cls));cls.lpfnWndProc=videoProc;cls.hInstance=GetModuleHandleW(0);
    cls.lpszClassName=L"GensKoVideoSettings";cls.hCursor=LoadCursor(0,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&cls);
    RECT parentRect;GetWindowRect(parent,&parentRect);int x=parentRect.left+30,y=parentRect.top+40;
    MONITORINFO mi;ZeroMemory(&mi,sizeof(mi));mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromWindow(parent,MONITOR_DEFAULTTONEAREST),&mi);
    x=std::max((int)mi.rcWork.left,std::min(x,(int)mi.rcWork.right-700));y=std::max((int)mi.rcWork.top,std::min(y,(int)mi.rcWork.bottom-470));
    HWND dlg=CreateWindowExW(WS_EX_DLGMODALFRAME,L"GensKoVideoSettings",L"화면 출력 및 셰이더",WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
       x,y,700,470,parent,0,GetModuleHandleW(0),0);
    if(!dlg)return;EnableWindow(parent,FALSE);SetFocus(GetDlgItem(dlg,VIDEO_PRESET));
    MSG msg;while(IsWindow(dlg)&&GetMessageW(&msg,0,0,0)>0){
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(dlg);break;}
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN){SendMessageW(dlg,WM_COMMAND,VIDEO_APPLY,0);continue;}
        if(!IsDialogMessageW(dlg,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    EnableWindow(parent,TRUE);SetForegroundWindow(parent);Renderer_Repaint(parent);
}
