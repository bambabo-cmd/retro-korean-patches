/* Gens KO, 2026-11-20: Unicode file paths and Korean UI. GPLv2. */
#define GENS_KO_API_IMPL
#include "ko_winapi.h"
#include <vector>
#include <cstring>
#include <algorithm>
#include "ui_ko.inc"

std::wstring KoWide(const char *s) {
    if (!s) return std::wstring();
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s,-1,NULL,0);
    UINT cp=CP_UTF8;
    if (!n) { cp=CP_ACP; n=MultiByteToWideChar(cp,0,s,-1,NULL,0); }
    if (n<=0) return std::wstring();
    std::wstring out((size_t)n,L'\0');
    MultiByteToWideChar(cp,0,s,-1,&out[0],n);out.resize((size_t)n-1);return out;
}
std::string KoUtf8(const wchar_t *s) {
    if(!s)return std::string();
    int n=WideCharToMultiByte(CP_UTF8,0,s,-1,NULL,0,NULL,NULL);
    if(n<=0)return std::string();
    std::string out((size_t)n,'\0');WideCharToMultiByte(CP_UTF8,0,s,-1,&out[0],n,NULL,NULL);out.resize((size_t)n-1);return out;
}
static DWORD copy_utf8(char *dst,DWORD cap,const wchar_t *src) {
    if(!dst || !cap)return 0;
    std::string s=KoUtf8(src);
    size_t n=std::min(s.size(),(size_t)cap-1);
    while(n && n<s.size() && (((unsigned char)s[n]&0xC0)==0x80))--n;
    memcpy(dst,s.data(),n);dst[n]=0;return (DWORD)n;
}
extern "C" const char *KoText(const char *key,const char *fallback) {
    if(key)for(size_t i=0;i<sizeof(ko_texts)/sizeof(ko_texts[0]);++i)if(!strcmp(key,ko_texts[i][0]))return ko_texts[i][1];
    if(fallback)for(size_t i=0;i<sizeof(ko_texts)/sizeof(ko_texts[0]);++i)if(!strcmp(fallback,ko_texts[i][0]))return ko_texts[i][1];
    return fallback?fallback:(key?key:"");
}
extern "C" int KoMessageBox(HWND w,const char *s,const char *t,UINT flags) {
    auto a=KoWide(KoText(s,s));auto b=KoWide(t?KoText(t,t):"Gens KO");
    return MessageBoxW(w,a.c_str(),b.c_str(),flags);
}
extern "C" BOOL KoSetWindowText(HWND w,const char *s){auto a=KoWide(KoText(s,s));return SetWindowTextW(w,a.c_str());}
extern "C" BOOL KoSetDlgItemText(HWND w,int id,const char *s){auto a=KoWide(KoText(s,s));return SetDlgItemTextW(w,id,a.c_str());}
extern "C" UINT KoGetDlgItemText(HWND w,int id,char *s,int n){std::vector<wchar_t>a((size_t)std::max(n,1));GetDlgItemTextW(w,id,a.data(),(int)a.size());return copy_utf8(s,n>0?(DWORD)n:0,a.data());}
extern "C" int KoGetWindowText(HWND w,char *s,int n){std::vector<wchar_t>a((size_t)std::max(n,1));GetWindowTextW(w,a.data(),(int)a.size());return (int)copy_utf8(s,n>0?(DWORD)n:0,a.data());}
extern "C" LRESULT KoGetListText(HWND w,int id,int item,char *s,int cap){
    if(!s||cap<=0)return LB_ERR;s[0]=0;
    LRESULT n=SendDlgItemMessageW(w,id,LB_GETTEXTLEN,item,0);
    if(n<0||n>32767)return LB_ERR;
    std::vector<wchar_t>v((size_t)n+1);
    if(SendDlgItemMessageW(w,id,LB_GETTEXT,item,(LPARAM)v.data())==LB_ERR)return LB_ERR;
    return copy_utf8(s,(DWORD)cap,v.data());
}
extern "C" BOOL KoInsertMenu(HMENU m,UINT pos,UINT flags,UINT_PTR id,const char *s){
    if(flags&MF_SEPARATOR)return InsertMenuW(m,pos,flags,id,NULL);
    if(flags&(MF_BITMAP|MF_OWNERDRAW))return InsertMenuA(m,pos,flags,id,s);
    auto a=KoWide(KoText(s,s));return InsertMenuW(m,pos,flags,id,a.c_str());
}
extern "C" HANDLE KoCreateFile(const char *s,DWORD access,DWORD share,LPSECURITY_ATTRIBUTES sec,DWORD how,DWORD flags,HANDLE templ){auto a=KoWide(s);return CreateFileW(a.c_str(),access,share,sec,how,flags,templ);}
extern "C" FILE *KoFopen(const char *s,const char *mode){auto a=KoWide(s),b=KoWide(mode);return _wfopen(a.c_str(),b.c_str());}
extern "C" BOOL KoDeleteFile(const char *s){auto a=KoWide(s);return DeleteFileW(a.c_str());}
extern "C" BOOL KoMoveFileEx(const char *s,const char *t,DWORD f){auto a=KoWide(s),b=KoWide(t);return MoveFileExW(a.c_str(),t?b.c_str():NULL,f);}
extern "C" DWORD KoGetFileAttributes(const char *s){auto a=KoWide(s);return GetFileAttributesW(a.c_str());}
extern "C" DWORD KoGetCurrentDirectory(DWORD n,char *s){wchar_t a[32768];DWORD r=GetCurrentDirectoryW(32768,a);if(!r||r>=32768){if(n&&s)*s=0;return 0;}auto t=KoUtf8(a);if(t.size()+1>n)return (DWORD)t.size()+1;return copy_utf8(s,n,a);}
extern "C" BOOL KoSetCurrentDirectory(const char *s){auto a=KoWide(s);return SetCurrentDirectoryW(a.c_str());}
static std::wstring filter_wide(const char *s){
    std::wstring out;if(!s)return out;
    size_t total=0;
    while(*s && total<32768){size_t n=strlen(s);auto w=KoWide(KoText(s,s));out+=w;out+=L'\0';s+=n+1;total+=n+1;}
    out+=L'\0';return out;
}
static BOOL file_dialog(LPOPENFILENAMEA a,bool save){
    if(!a||!a->lpstrFile||!a->nMaxFile)return FALSE;
    OPENFILENAMEW w={};w.lStructSize=sizeof(w);w.hwndOwner=a->hwndOwner;w.hInstance=a->hInstance;
    auto filter=filter_wide(a->lpstrFilter),dir=KoWide(a->lpstrInitialDir),title=KoWide(KoText(a->lpstrTitle,a->lpstrTitle)),ext=KoWide(a->lpstrDefExt);
    std::vector<wchar_t> file(32768,L'\0');auto start=KoWide(a->lpstrFile);if(start.size()<file.size())std::copy(start.begin(),start.end(),file.begin());
    w.lpstrFilter=filter.empty()?NULL:filter.c_str();w.nFilterIndex=a->nFilterIndex;w.lpstrFile=file.data();w.nMaxFile=(DWORD)file.size();
    w.lpstrInitialDir=a->lpstrInitialDir?dir.c_str():NULL;w.lpstrTitle=a->lpstrTitle?title.c_str():NULL;w.lpstrDefExt=a->lpstrDefExt?ext.c_str():NULL;
    w.Flags=(a->Flags & ~(OFN_ENABLEHOOK|OFN_ENABLETEMPLATE|OFN_ENABLETEMPLATEHANDLE|OFN_ALLOWMULTISELECT))|OFN_EXPLORER|OFN_NOCHANGEDIR;
    if(!(save?GetSaveFileNameW(&w):GetOpenFileNameW(&w)))return FALSE;
    std::string path=KoUtf8(file.data());
    if(path.size()>=a->nMaxFile){MessageBoxW(a->hwndOwner,L"파일 경로가 너무 깁니다. 더 짧은 폴더로 옮겨 주세요.",L"Gens KO",MB_OK|MB_ICONERROR);return FALSE;}
    memcpy(a->lpstrFile,path.c_str(),path.size()+1);a->nFilterIndex=w.nFilterIndex;
    size_t sep=path.find_last_of("/\\");a->nFileOffset=(WORD)(sep==std::string::npos?0:sep+1);size_t dot=path.find_last_of('.');a->nFileExtension=(WORD)(dot==std::string::npos?0:dot+1);
    if(a->lpstrFileTitle&&a->nMaxFileTitle){auto b=KoWide(path.c_str()+a->nFileOffset);copy_utf8(a->lpstrFileTitle,a->nMaxFileTitle,b.c_str());}
    return TRUE;
}
extern "C" BOOL KoGetOpenFileName(LPOPENFILENAMEA a){return file_dialog(a,false);}
extern "C" BOOL KoGetSaveFileName(LPOPENFILENAMEA a){return file_dialog(a,true);}
extern "C" DWORD KoGetPrivateProfileString(const char *sec,const char *key,const char *def,char *out,DWORD cap,const char *file){
    auto a=KoWide(sec),b=KoWide(key),c=KoWide(def),d=KoWide(file);std::vector<wchar_t>v(32768);
    DWORD r=GetPrivateProfileStringW(sec?a.c_str():NULL,key?b.c_str():NULL,def?c.c_str():NULL,v.data(),(DWORD)v.size(),file?d.c_str():NULL);
    if(sec&&key)return copy_utf8(out,cap,v.data());
    if(!out||cap<2)return 0;DWORD used=0;const wchar_t*p=v.data();
    while(*p&&used+1<cap){std::string x=KoUtf8(p);if(used+x.size()+2>cap)break;memcpy(out+used,x.c_str(),x.size()+1);used+=(DWORD)x.size()+1;p+=wcslen(p)+1;}
    out[used]=0;return used;
}
extern "C" UINT KoGetPrivateProfileInt(const char *sec,const char *key,INT def,const char *file){auto a=KoWide(sec),b=KoWide(key),d=KoWide(file);return GetPrivateProfileIntW(a.c_str(),b.c_str(),def,d.c_str());}
extern "C" BOOL KoWritePrivateProfileString(const char *sec,const char *key,const char *val,const char *file){
    auto a=KoWide(sec),b=KoWide(key),c=KoWide(val),d=KoWide(file);
    if(file&&GetFileAttributesW(d.c_str())==INVALID_FILE_ATTRIBUTES){HANDLE h=CreateFileW(d.c_str(),GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(h!=INVALID_HANDLE_VALUE){DWORD n;const WORD bom=0xFEFF;WriteFile(h,&bom,2,&n,NULL);CloseHandle(h);}}
    return WritePrivateProfileStringW(sec?a.c_str():NULL,key?b.c_str():NULL,val?c.c_str():NULL,file?d.c_str():NULL);
}
extern "C" HMODULE KoLoadLibrary(const char *s){auto a=KoWide(s);return LoadLibraryW(a.c_str());}
extern "C" void KoInitializePaths(void){wchar_t path[32768];DWORD n=GetModuleFileNameW(NULL,path,32768);if(n&&n<32768){wchar_t*p=wcsrchr(path,L'\\');if(p){*p=0;SetCurrentDirectoryW(path);}}}
extern "C" void KoTraceStart(void){
    FILE *f=_wfopen(L"Gens-KO-startup.log",L"wb");
    if(f){fputs("Gens KO 2.14 r2 experimental Windows build\r\n",f);fclose(f);}
}
extern "C" void KoTrace(const char *message){
    FILE *f=_wfopen(L"Gens-KO-startup.log",L"ab");if(!f)return;
    SYSTEMTIME t;GetLocalTime(&t);
    fprintf(f,"%02u:%02u:%02u.%03u %s\r\n",t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,message?message:"");
    fclose(f);
}
