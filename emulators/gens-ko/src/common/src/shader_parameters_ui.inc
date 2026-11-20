/* Gens KO shader parameter editor. GPL-2.0; see LICENSE.txt.
   Included by renderer_gl.cpp after its engine and basic UI helpers. */
#include <cwchar>
#include <cerrno>

static std::wstring shaderParameterSection() {
    std::wstring key=presetPath;
    wchar_t absolute[32768]={0};
    DWORD n=GetFullPathNameW(key.c_str(),32768,absolute,0);
    if(n&&n<32768)key=absolute;
    if(!key.empty())CharLowerBuffW(&key[0],(DWORD)key.size());
    unsigned long long hash=14695981039346656037ull;
    for(size_t i=0;i<key.size();++i){hash^=(unsigned short)key[i];hash*=1099511628211ull;}
    std::wostringstream name;name<<L"Shader-"<<std::hex<<hash;return name.str();
}
static std::wstring parameterText(float value) {
    std::wostringstream text;text.precision(7);text<<value;return text.str();
}
static bool parameterNumber(const wchar_t *text,float minimum,float maximum,float &value) {
    if(!text||!*text)return false;
    errno=0;wchar_t *end=0;double number=wcstod(text,&end);
    if(end==text)return false;
    while(end&&(*end==L' '||*end==L'\t'))++end;
    if(!end||*end||errno||!std::isfinite(number))return false;
    float parsed=(float)number;
    if(!std::isfinite(parsed)||parsed<minimum||parsed>maximum)return false;
    value=parsed;return true;
}
static void loadSavedShaderParameters() {
    if(presetPath.empty())return;
    std::wstring section=shaderParameterSection();
    std::wstring file=baseDir+L"Gens-KO-shaders.ini";
    wchar_t storedPath[32768]={0};
    GetPrivateProfileStringW(section.c_str(),L"Preset",L"",storedPath,32768,file.c_str());
    if(_wcsicmp(storedPath,presetPath.c_str())!=0)return;
    for(size_t i=0;i<shaderParameters.size();++i){
        const ShaderParameter &spec=shaderParameters[i];
        wchar_t text[128]={0};float value=0;
        GetPrivateProfileStringW(section.c_str(),wide(spec.name).c_str(),L"",text,128,file.c_str());
        if(parameterNumber(text,spec.minimum,spec.maximum,value))parameters[spec.name]=value;
    }
}
static bool saveShaderParameters() {
    if(presetPath.empty())return true;
    std::wstring section=shaderParameterSection(),file=baseDir+L"Gens-KO-shaders.ini";
    /* Profile W APIs create ANSI files unless an existing UTF-16 BOM says
       otherwise. Preserve Unicode preset paths on any system code page. */
    HANDLE initial=CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
    if(initial!=INVALID_HANDLE_VALUE){
        const unsigned char bom[2]={0xff,0xfe};DWORD written=0;
        BOOL made=WriteFile(initial,bom,2,&written,0);CloseHandle(initial);
        if(!made||written!=2){DeleteFileW(file.c_str());return false;}
    }else if(GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    bool ok=WritePrivateProfileStringW(section.c_str(),L"Preset",presetPath.c_str(),file.c_str())!=0;
    for(size_t i=0;i<shaderParameters.size();++i){
        const ShaderParameter &spec=shaderParameters[i];
        std::map<std::string,float>::const_iterator found=parameters.find(spec.name);
        if(found!=parameters.end())
            ok=(WritePrivateProfileStringW(section.c_str(),wide(spec.name).c_str(),parameterText(found->second).c_str(),file.c_str())!=0)&&ok;
    }
    return ok;
}
static std::wstring parameterLabel(const ShaderParameter &spec) {
    struct Name {const char *key;const wchar_t *label;};
    static const Name names[]={
        {"DITHER",L"디더링 섞기"},{"SOFT_ALL",L"전체 부드러움"},
        {"LUMA_BLUR",L"밝기 신호 번짐"},{"LUMA_TAIL",L"밝기 신호 꼬리"},
        {"CHROMA_BLUR",L"색 신호 번짐"},{"CHROMA_TAIL",L"색 신호 꼬리"},
        {"PEAKING",L"윤곽 선명도"},{"SATURATION",L"채도"},{"HUE",L"색조"},
        {"LV_BLACK",L"검정 수준"},{"LV_WHITE",L"흰색 수준"},
        {"GAMMA_IN",L"입력 감마"},{"WP_R",L"화이트 밸런스: 빨강"},
        {"WP_G",L"화이트 밸런스: 초록"},{"WP_B",L"화이트 밸런스: 파랑"},
        {"VERT_BLUR",L"세로 부드러움"},{"GLOW",L"빛 번짐 강도"},{"GLOW_R",L"빛 번짐 반경"},
        {"MASK_TYPE",L"마스크 종류 (0: RGB 줄 / 1: 슬롯)"},
        {"MASK_PITCH",L"RGB 줄무늬 간격 (0: 자동)"},{"MASK_SIZE",L"자동 마스크 크기"},
        {"CONVERGE",L"RGB 정렬 어긋남"},{"RGB_BLUR_X",L"가로 RGB 번짐"},
        {"RGB_BLUR_Y",L"세로 RGB 번짐"},{"FINAL_BLUR",L"마지막 부드러움"},
        {"BRIGHT",L"화면 밝기"},{"SLOT_GAP",L"슬롯 틈 비율"},
        {"SLOT_DEPTH",L"슬롯 틈 진하기"},{"SLOT_EDGE",L"슬롯 가장자리 부드러움"},
        {"SLOT_LOCK",L"슬롯을 주사선에 맞춤 (0: 끔 / 1: 켬)"},
        {"SLOT_LINES",L"슬롯 높이 (주사선 수)"},{"SLOT_SHAPE",L"슬롯 옆면을 곧게"},
        {"SLOT_VADJ",L"슬롯 세로 간격 보정"},{"AFTERGLOW",L"형광체 잔광 강도 (0: 끔)"},
        {"AG_TIME",L"잔광 시간 (ms)"},{"AG_RED",L"빨강 잔광 배율"},
        {"CURVATURE",L"화면 곡률 (0: 평면)"},{"CORNER",L"모서리 둥글기"},
        {"VIGNETTE",L"가장자리 어두워짐"},{"MASK_CURVE",L"마스크도 곡률을 따름"},
        {"PRESCALE",L"내부 해상도 배율"},{"RASTER_LINES",L"주사선 수 (0: 자동)"},
        {"CONTENT_SCALE",L"입력 화면 배율 (0: 자동)"}
    };
    for(size_t i=0;i<sizeof(names)/sizeof(names[0]);++i)if(spec.name==names[i].key)return names[i].label;
    return wide(spec.label.empty()?spec.name:spec.label);
}
enum { SHPARAM_LIST=5220,SHPARAM_VALUE,SHPARAM_RANGE,SHPARAM_APPLY,SHPARAM_CLOSE,SHPARAM_ONE_DEFAULT,SHPARAM_ALL_DEFAULT,SHPARAM_NAME };
struct ShaderParameterDialog {
    std::vector<size_t> visible;
    std::map<std::string,float> pending;
    int selected;
    HFONT font;
    ShaderParameterDialog():selected(-1),font(0){}
};
static HWND parameterControl(HWND owner,const wchar_t *type,const wchar_t *text,DWORD style,
                             int x,int y,int w,int h,int id,HFONT font) {
    HWND child=CreateWindowExW((type==std::wstring(L"EDIT")||type==std::wstring(L"LISTBOX"))?WS_EX_CLIENTEDGE:0,
       type,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,owner,(HMENU)(INT_PTR)id,GetModuleHandleW(0),0);
    SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE);return child;
}
static void parameterRows(HWND dlg,ShaderParameterDialog &data) {
    HWND list=GetDlgItem(dlg,SHPARAM_LIST);SendMessageW(list,WM_SETREDRAW,FALSE,0);SendMessageW(list,LB_RESETCONTENT,0,0);
    for(size_t row=0;row<data.visible.size();++row){
        const ShaderParameter &spec=shaderParameters[data.visible[row]];
        std::wstring label=parameterLabel(spec)+L"  =  "+parameterText(data.pending[spec.name]);
        SendMessageW(list,LB_ADDSTRING,0,(LPARAM)label.c_str());
    }
    SendMessageW(list,LB_SETCURSEL,data.selected,0);SendMessageW(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,0,TRUE);
}
static void parameterSelection(HWND dlg,ShaderParameterDialog &data) {
    if(data.selected<0||data.selected>=(int)data.visible.size())return;
    const ShaderParameter &spec=shaderParameters[data.visible[data.selected]];
    SetDlgItemTextW(dlg,SHPARAM_NAME,(parameterLabel(spec)+L"  ["+wide(spec.name)+L"]").c_str());
    SetDlgItemTextW(dlg,SHPARAM_VALUE,parameterText(data.pending[spec.name]).c_str());
    std::wstring range=L"범위 "+parameterText(spec.minimum)+L" ~ "+parameterText(spec.maximum)+
        L"  /  기본값 "+parameterText(spec.default_value);
    SetDlgItemTextW(dlg,SHPARAM_RANGE,range.c_str());
}
static bool parameterCommitEdit(HWND dlg,ShaderParameterDialog &data) {
    if(data.selected<0||data.selected>=(int)data.visible.size())return true;
    const ShaderParameter &spec=shaderParameters[data.visible[data.selected]];
    wchar_t text[128]={0};float value=0;GetDlgItemTextW(dlg,SHPARAM_VALUE,text,128);
    if(!parameterNumber(text,spec.minimum,spec.maximum,value)){
        std::wstring error=L"표시된 범위의 숫자를 입력해 주세요.\n"+parameterText(spec.minimum)+L" ~ "+parameterText(spec.maximum);
        MessageBoxW(dlg,error.c_str(),L"셰이더 설정",MB_OK|MB_ICONINFORMATION);
        SetFocus(GetDlgItem(dlg,SHPARAM_VALUE));return false;
    }
    data.pending[spec.name]=value;return true;
}
static LRESULT CALLBACK parameterProc(HWND dlg,UINT msg,WPARAM wp,LPARAM lp) {
    ShaderParameterDialog *data=(ShaderParameterDialog *)GetWindowLongPtrW(dlg,GWLP_USERDATA);
    if(msg==WM_CREATE){
        data=(ShaderParameterDialog *)((CREATESTRUCTW *)lp)->lpCreateParams;
        SetWindowLongPtrW(dlg,GWLP_USERDATA,(LONG_PTR)data);
        data->font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
        parameterControl(dlg,L"STATIC",L"항목을 선택하고 값을 바꾸세요. 적용한 값은 프리셋별로 기억합니다.",0,18,14,660,26,0,data->font);
        parameterControl(dlg,L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,18,48,660,270,SHPARAM_LIST,data->font);
        parameterControl(dlg,L"STATIC",L"",0,18,333,660,26,SHPARAM_NAME,data->font);
        parameterControl(dlg,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,18,366,120,30,SHPARAM_VALUE,data->font);
        parameterControl(dlg,L"STATIC",L"",0,152,370,525,26,SHPARAM_RANGE,data->font);
        parameterControl(dlg,L"BUTTON",L"선택 기본값",WS_TABSTOP|BS_PUSHBUTTON,18,416,135,34,SHPARAM_ONE_DEFAULT,data->font);
        parameterControl(dlg,L"BUTTON",L"모두 기본값",WS_TABSTOP|BS_PUSHBUTTON,163,416,135,34,SHPARAM_ALL_DEFAULT,data->font);
        parameterControl(dlg,L"BUTTON",L"적용·저장",WS_TABSTOP|BS_DEFPUSHBUTTON,440,416,114,34,SHPARAM_APPLY,data->font);
        parameterControl(dlg,L"BUTTON",L"닫기",WS_TABSTOP|BS_PUSHBUTTON,564,416,114,34,SHPARAM_CLOSE,data->font);
        data->selected=data->visible.empty()?-1:0;parameterRows(dlg,*data);parameterSelection(dlg,*data);return 0;
    }
    if(msg==WM_COMMAND&&data){
        int id=LOWORD(wp);
        if(id==SHPARAM_LIST&&HIWORD(wp)==LBN_SELCHANGE){
            int row=(int)SendDlgItemMessageW(dlg,SHPARAM_LIST,LB_GETCURSEL,0,0);
            if(!parameterCommitEdit(dlg,*data)){SendDlgItemMessageW(dlg,SHPARAM_LIST,LB_SETCURSEL,data->selected,0);return 0;}
            data->selected=row;parameterRows(dlg,*data);parameterSelection(dlg,*data);return 0;
        }
        if(id==SHPARAM_ONE_DEFAULT||id==SHPARAM_ALL_DEFAULT){
            if(id==SHPARAM_ONE_DEFAULT&&data->selected>=0){
                const ShaderParameter &spec=shaderParameters[data->visible[data->selected]];data->pending[spec.name]=spec.default_value;
            }else if(id==SHPARAM_ALL_DEFAULT){
                for(size_t i=0;i<data->visible.size();++i){const ShaderParameter &spec=shaderParameters[data->visible[i]];data->pending[spec.name]=spec.default_value;}
            }
            parameterRows(dlg,*data);parameterSelection(dlg,*data);return 0;
        }
        if(id==SHPARAM_APPLY){
            if(!parameterCommitEdit(dlg,*data))return 0;
            parameters=data->pending;
            resetFeedbackHistory();
            if(!saveShaderParameters())MessageBoxW(dlg,L"값을 적용했지만 설정 파일을 저장하지 못했습니다. 쓰기 가능한 폴더에서 실행해 주세요.",L"Gens KO",MB_OK|MB_ICONWARNING);
            parameterRows(dlg,*data);Renderer_Repaint(window);return 0;
        }
        if(id==SHPARAM_CLOSE||id==IDCANCEL){DestroyWindow(dlg);return 0;}
    }
    if(msg==WM_CLOSE){DestroyWindow(dlg);return 0;}
    if(msg==WM_DESTROY){if(data&&data->font){DeleteObject(data->font);data->font=0;}return 0;}
    return DefWindowProcW(dlg,msg,wp,lp);
}
static void showShaderParameters(HWND parent) {
    if(!context||passes.empty()){
        MessageBoxW(parent,L"OpenGL 출력에서 셰이더를 먼저 적용해 주세요. GDI 호환 출력에서는 셰이더를 조절할 수 없습니다.",L"셰이더 세부 설정",MB_OK|MB_ICONINFORMATION);return;
    }
    ShaderParameterDialog data;data.pending=parameters;
    for(size_t i=0;i<shaderParameters.size();++i)
        if(shaderParameters[i].maximum>shaderParameters[i].minimum)data.visible.push_back(i);
    if(data.visible.empty()){
        MessageBoxW(parent,L"먼저 셰이더를 선택해 적용해 주세요. 이 셰이더에 조절할 항목이 없을 수도 있습니다.",L"셰이더 세부 설정",MB_OK|MB_ICONINFORMATION);return;
    }
    WNDCLASSW cls;ZeroMemory(&cls,sizeof(cls));cls.lpfnWndProc=parameterProc;cls.hInstance=GetModuleHandleW(0);
    cls.lpszClassName=L"GensKoShaderParameters";cls.hCursor=LoadCursor(0,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);RegisterClassW(&cls);
    RECT r;GetWindowRect(parent,&r);MONITORINFO mi;ZeroMemory(&mi,sizeof(mi));mi.cbSize=sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(parent,MONITOR_DEFAULTTONEAREST),&mi);
    int x=std::max((int)mi.rcWork.left,std::min((int)r.left+15,(int)mi.rcWork.right-714));
    int y=std::max((int)mi.rcWork.top,std::min((int)r.top+15,(int)mi.rcWork.bottom-500));
    HWND dlg=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,L"셰이더 세부 설정",WS_CAPTION|WS_SYSMENU|WS_VISIBLE,x,y,714,500,parent,0,GetModuleHandleW(0),&data);
    if(!dlg)return;
    EnableWindow(parent,FALSE);SetFocus(GetDlgItem(dlg,SHPARAM_LIST));MSG msg;
    while(IsWindow(dlg)&&GetMessageW(&msg,0,0,0)>0){
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(dlg);break;}
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_RETURN){SendMessageW(dlg,WM_COMMAND,SHPARAM_APPLY,0);continue;}
        if(!IsDialogMessageW(dlg,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    EnableWindow(parent,TRUE);SetForegroundWindow(parent);
}
