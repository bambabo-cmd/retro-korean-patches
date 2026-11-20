/* Gens KO: modified 2026-11-20. See CHANGELOG_KO.txt and original license notices below. */
/* Modified 2026-11-20: focus-safe polling, hotplug, validated bindings and XInput.
 * Gens original copyright notices and GPL terms are in LICENSE.txt. */
#include <stdio.h>
#include <string.h>
#include "resource.h"
#include "G_Input.h"
#include "modern_input.h"
#include "io.h"
#include "G_main.h"

#define KEYDOWN(key) (Keys[key] & 0x80)
#define MAX_JOYS 8

LPDIRECTINPUT lpDI = NULL;
LPDIRECTINPUTDEVICE lpDIDKeyboard = NULL;
LPDIRECTINPUTDEVICE lpDIDMouse = NULL;
char Phrase[1024];
int Nb_Joys = 0;
static IDirectInputDevice2 *Joy_ID[MAX_JOYS] = {NULL};
static DIJOYSTATE Joy_State[MAX_JOYS];
static GUID Joy_Guid[MAX_JOYS];
static bool Joy_HasGuid[MAX_JOYS] = {false};
static bool Joy_Seen[MAX_JOYS] = {false};
static bool Joy_Connected[MAX_JOYS] = {false};
static wchar_t Joy_Name[MAX_JOYS][MAX_PATH];
static HWND Input_Window = NULL;
static DWORD Next_Device_Scan = 0;
static bool Input_WasForeground = false;
long MouseX, MouseY;
unsigned char Keys[256];
unsigned char Kaillera_Keys[16];
int KeyIndex=0;
int Player=0;

struct K_Def Keys_Def[8] = {
    {DIK_V,DIK_F,DIK_C,DIK_X,DIK_Z,DIK_D,DIK_S,DIK_A,
     DIK_UP,DIK_DOWN,DIK_LEFT,DIK_RIGHT},
    {DIK_U,DIK_T,DIK_K,DIK_L,DIK_M,DIK_I,DIK_O,DIK_P,
     DIK_Y,DIK_H,DIK_G,DIK_J}
};

static void Clear_Joy_State(int i) {
    memset(&Joy_State[i],0,sizeof(Joy_State[i]));
    for (int p=0;p<4;++p) Joy_State[i].rgdwPOV[p]=0xFFFFFFFFUL;
}

int String_Size(char *text) { return (int)strlen(text); }

void Input_ClearState(void) {
    // Release only button outputs; preserve the emulated controller handshake.
#define RELEASE_PAD(p) \
    Controller_##p##_Up=Controller_##p##_Down=Controller_##p##_Left=Controller_##p##_Right=1; \
    Controller_##p##_Start=Controller_##p##_Mode=1; \
    Controller_##p##_A=Controller_##p##_B=Controller_##p##_C=1; \
    Controller_##p##_X=Controller_##p##_Y=Controller_##p##_Z=1
    RELEASE_PAD(1);RELEASE_PAD(2);RELEASE_PAD(1B);RELEASE_PAD(1C);
    RELEASE_PAD(1D);RELEASE_PAD(2B);RELEASE_PAD(2C);RELEASE_PAD(2D);
#undef RELEASE_PAD
    memset(Keys,0,sizeof(Keys));
    for(int i=0;i<MAX_JOYS;++i) Clear_Joy_State(i);
    ModernInput_ClearState();
    Input_WasForeground=false;
}

HWND Input_GetWindow(void) { return Input_Window; }
void Input_ResetJoystickPreferences(void) {
    for(int i=0;i<MAX_JOYS;++i) if(!Joy_ID[i]) { Joy_HasGuid[i]=false;Joy_Name[i][0]=0; }
}

int Input_HasFocus(void) {
    HWND f=GetForegroundWindow();
    return Input_Window && (f==Input_Window || IsChild(Input_Window,f));
}

void End_Input(void) {
    Input_ClearState();
    if(lpDIDMouse) { lpDIDMouse->Unacquire(); lpDIDMouse->Release(); lpDIDMouse=NULL; }
    if(lpDIDKeyboard) { lpDIDKeyboard->Unacquire(); lpDIDKeyboard->Release(); lpDIDKeyboard=NULL; }
    for(int i=0;i<MAX_JOYS;++i) {
        if(Joy_ID[i]) { Joy_ID[i]->Unacquire(); Joy_ID[i]->Release(); Joy_ID[i]=NULL; }
        Joy_Connected[i]=false;
    }
    if(lpDI) { lpDI->Release(); lpDI=NULL; }
    Input_Window=NULL;
    Nb_Joys=0;
}

BOOL CALLBACK InitJoystick(LPCDIDEVICEINSTANCE inst,LPVOID owner) {
    int slot=-1;
    for(int i=0;i<MAX_JOYS;++i)
        if(Joy_HasGuid[i] && !memcmp(&Joy_Guid[i],&inst->guidInstance,sizeof(GUID))) { slot=i;break; }
    if(slot<0) for(int i=0;i<MAX_JOYS;++i) if(!Joy_HasGuid[i]) { slot=i;break; }
    if(slot<0) return DIENUM_CONTINUE;
    Joy_Seen[slot]=true;
    if(Joy_ID[slot]) { Joy_Connected[slot]=true;return DIENUM_CONTINUE; }
    LPDIRECTINPUTDEVICE initial=NULL;
    if(FAILED(lpDI->CreateDevice(inst->guidInstance,&initial,NULL))) return DIENUM_CONTINUE;
    HRESULT hr=initial->QueryInterface(IID_IDirectInputDevice2,(void**)&Joy_ID[slot]);
    initial->Release();
    if(FAILED(hr)) { Joy_ID[slot]=NULL;return DIENUM_CONTINUE; }
    hr=Joy_ID[slot]->SetDataFormat(&c_dfDIJoystick);
    if(SUCCEEDED(hr)) hr=Joy_ID[slot]->SetCooperativeLevel((HWND)owner,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND);
    if(FAILED(hr)) { Joy_ID[slot]->Release();Joy_ID[slot]=NULL;return DIENUM_CONTINUE; }
    static const DWORD offsets[8]={DIJOFS_X,DIJOFS_Y,DIJOFS_Z,DIJOFS_RX,DIJOFS_RY,DIJOFS_RZ,DIJOFS_SLIDER(0),DIJOFS_SLIDER(1)};
    for(int a=0;a<8;++a) {
        DIPROPRANGE r;memset(&r,0,sizeof(r));
        r.diph.dwSize=sizeof(r);r.diph.dwHeaderSize=sizeof(r.diph);
        r.diph.dwObj=offsets[a];r.diph.dwHow=DIPH_BYOFFSET;r.lMin=-1000;r.lMax=1000;
        Joy_ID[slot]->SetProperty(DIPROP_RANGE,&r.diph);
    }
    Joy_Guid[slot]=inst->guidInstance;Joy_HasGuid[slot]=true;
#ifdef UNICODE
    lstrcpynW(Joy_Name[slot],inst->tszInstanceName,MAX_PATH);
#else
    MultiByteToWideChar(CP_ACP,0,inst->tszInstanceName,-1,Joy_Name[slot],MAX_PATH);
#endif
    if(!Joy_Name[slot][0]) lstrcpynW(Joy_Name[slot],L"DirectInput 게임패드",MAX_PATH);
    if(slot>=Nb_Joys) Nb_Joys=slot+1;
    Clear_Joy_State(slot);
    Joy_Connected[slot]=true;
    if(Input_HasFocus()) Joy_ID[slot]->Acquire();
    return DIENUM_CONTINUE;
}

void Input_RefreshDevices(void) {
    if(!lpDI || !Input_Window) return;
    memset(Joy_Seen,0,sizeof(Joy_Seen));
    HRESULT hr=lpDI->EnumDevices(DIDEVTYPE_JOYSTICK,InitJoystick,Input_Window,DIEDFL_ATTACHEDONLY);
    if(SUCCEEDED(hr)) for(int i=0;i<MAX_JOYS;++i) if(!Joy_Seen[i] && Joy_ID[i]) {
        Joy_ID[i]->Unacquire();Joy_ID[i]->Release();Joy_ID[i]=NULL;
        Joy_Connected[i]=false;Clear_Joy_State(i);
    }
    Next_Device_Scan=GetTickCount()+2000;
}

int Init_Input(HINSTANCE instance,HWND owner) {
    End_Input();Input_Window=owner;
    HRESULT hr=DirectInputCreate(instance,DIRECTINPUT_VERSION,&lpDI,NULL);
    if(FAILED(hr)) { Input_Window=NULL;return 0; }
    hr=lpDI->CreateDevice(GUID_SysKeyboard,&lpDIDKeyboard,NULL);
    if(SUCCEEDED(hr)) hr=lpDIDKeyboard->SetCooperativeLevel(owner,DISCL_NONEXCLUSIVE|DISCL_FOREGROUND);
    if(SUCCEEDED(hr)) hr=lpDIDKeyboard->SetDataFormat(&c_dfDIKeyboard);
    if(FAILED(hr)) { End_Input();return 0; }
    Input_RefreshDevices();
    if(Input_HasFocus()) lpDIDKeyboard->Acquire();
    return 1;
}

void Restore_Input(void) {
    if(!Input_HasFocus()) return;
    if(lpDIDKeyboard) lpDIDKeyboard->Acquire();
    for(int i=0;i<MAX_JOYS;++i) if(Joy_ID[i]) Joy_ID[i]->Acquire();
}

void Update_Input(void) {
    DWORD now=GetTickCount();
    if((LONG)(now-Next_Device_Scan)>=0) Input_RefreshDevices();
    bool foreground=Input_HasFocus()!=0;
    ModernInput_Poll(foreground);
    if(!foreground) {
        memset(Keys,0,sizeof(Keys));
        for(int i=0;i<MAX_JOYS;++i) Clear_Joy_State(i);
        if(Input_WasForeground) {
            if(lpDIDKeyboard) lpDIDKeyboard->Unacquire();
            for(int i=0;i<MAX_JOYS;++i) if(Joy_ID[i]) Joy_ID[i]->Unacquire();
        }
        Input_WasForeground=false;
        return;
    }
    if(!Input_WasForeground) Restore_Input();
    Input_WasForeground=true;
    memset(Keys,0,sizeof(Keys));
    if(lpDIDKeyboard) {
        HRESULT hr=lpDIDKeyboard->GetDeviceState(sizeof(Keys),Keys);
        if(hr==DIERR_INPUTLOST) { lpDIDKeyboard->Acquire();hr=lpDIDKeyboard->GetDeviceState(sizeof(Keys),Keys); }
        if(FAILED(hr)) memset(Keys,0,sizeof(Keys));
    }
    for(int i=0;i<MAX_JOYS;++i) {
        Clear_Joy_State(i);
        if(!Joy_ID[i]) continue;
        Joy_ID[i]->Poll();
        HRESULT hr=Joy_ID[i]->GetDeviceState(sizeof(Joy_State[i]),&Joy_State[i]);
        if(hr==DIERR_INPUTLOST) {
            Joy_ID[i]->Acquire();Joy_ID[i]->Poll();
            hr=Joy_ID[i]->GetDeviceState(sizeof(Joy_State[i]),&Joy_State[i]);
        }
        if(FAILED(hr)) Clear_Joy_State(i);
        if(hr==DIERR_NOTINITIALIZED || hr==DIERR_DEVICENOTREG) Joy_Connected[i]=false;
    }
}

int Input_JoystickConnected(int slot) { return slot>=0 && slot<MAX_JOYS && Joy_Connected[slot]; }
int Input_JoystickKnown(int slot) { return slot>=0 && slot<MAX_JOYS && Joy_HasGuid[slot]; }
void Input_JoystickName(int slot,wchar_t *out,unsigned cap) {
    if(!out || !cap) return;
    if(slot>=0 && slot<MAX_JOYS && Joy_Name[slot][0]) lstrcpynW(out,Joy_Name[slot],cap);
    else lstrcpynW(out,L"DirectInput 게임패드",cap);
}
void Input_JoystickGuid(int slot,char *out,unsigned cap) {
    if(!out || cap<39) return;
    out[0]=0;
    if(slot<0 || slot>=MAX_JOYS || !Joy_HasGuid[slot]) return;
    const GUID &g=Joy_Guid[slot];
    sprintf(out,"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        (unsigned long)g.Data1,g.Data2,g.Data3,g.Data4[0],g.Data4[1],g.Data4[2],g.Data4[3],g.Data4[4],g.Data4[5],g.Data4[6],g.Data4[7]);
}
void Input_SetJoystickGuid(int slot,const char *text) {
    if(slot<0 || slot>=MAX_JOYS || !text || Joy_ID[slot]) return;
    unsigned v[11];
    if(sscanf(text,"{%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x}",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9],&v[10])!=11) return;
    GUID g;g.Data1=v[0];g.Data2=(WORD)v[1];g.Data3=(WORD)v[2];
    for(int i=0;i<8;++i) g.Data4[i]=(BYTE)v[i+3];
    Joy_Guid[slot]=g;Joy_HasGuid[slot]=true;
}

int Check_Key_Pressed(unsigned int key) {
    if(!GensInput::ValidCode(key) || key==0) return 0;
    if(key<0x100) return (Keys[key]&0x80)!=0;
    if((key&0xF000)==0x2000) return ModernInput_CheckXInput(key);
    unsigned slot=(key>>8)&15,value=key&255;
    if(slot>=MAX_JOYS || !Joy_ID[slot] || !Joy_Connected[slot]) return 0;
    const DIJOYSTATE &s=Joy_State[slot];
    if((value&0xC0)==0x80) return GensInput::PovPressed(s.rgdwPOV[(value>>4)&3],value&15);
    if(value>=0x10 && value<0x30) return (s.rgbButtons[value-0x10]&0x80)!=0;
    int dead=ModernInput_Deadzone();
    if(value>=0x40 && value<0x50) {
        const LONG axes[8]={s.lX,s.lY,s.lZ,s.lRx,s.lRy,s.lRz,s.rglSlider[0],s.rglSlider[1]};
        unsigned index=(value-0x40)/2;
        return GensInput::AxisPressed(axes[index],(value&1)!=0,1000,dead);
    }
    switch(value) {
    case 1:return GensInput::AxisPressed(s.lY,false,1000,dead);
    case 2:return GensInput::AxisPressed(s.lY,true,1000,dead);
    case 3:return GensInput::AxisPressed(s.lX,false,1000,dead);
    case 4:return GensInput::AxisPressed(s.lX,true,1000,dead);
    default:return 0;
    }
}

unsigned Input_FirstPressedCode(int filter) {
    if(filter<=0) for(unsigned i=1;i<256;++i) if(KEYDOWN(i)) return i;
    if(filter==-1 || (filter&0xF000)==0x2000) {
        int first=filter==-1?0:((filter>>8)&15),last=filter==-1?4:first+1;
        for(int i=first;i<last;++i) { unsigned key=ModernInput_FirstXInput(i);if(key) return key; }
    }
    if(filter==-1 || (filter&0xF000)==0x1000) {
        int first=filter==-1?0:((filter>>8)&15),last=filter==-1?MAX_JOYS:first+1;
        if(first<0 || last>MAX_JOYS) return 0;
        for(int i=first;i<last;++i) if(Joy_ID[i]) {
            unsigned base=0x1000|(i<<8);
            /* Prefer hats/buttons to axes; diagonals still bind a cardinal component. */
            for(unsigned h=0;h<4;++h) for(unsigned d=1;d<=4;++d)
                if(Check_Key_Pressed(base|0x80|(h<<4)|d)) return base|0x80|(h<<4)|d;
            for(unsigned b=0;b<32;++b) if(Check_Key_Pressed(base|(0x10+b))) return base|(0x10+b);
            for(unsigned d=1;d<=4;++d) if(Check_Key_Pressed(base|d)) return base|d;
            for(unsigned a=4;a<16;++a) if(Check_Key_Pressed(base|(0x40+a))) return base|(0x40+a);
        }
    }
    return 0;
}

unsigned int Get_Key(void) {
    Update_Input();unsigned key=Input_FirstPressedCode(-1);
    return key?key:(unsigned)-1;
}

void Update_Controllers()
{

	Update_Input();

	if (ModernInput_ActionPressed(0, GensInput::Up))
	{
		Controller_1_Up = 0;
		Controller_1_Down = 1;
	}
	else
	{
		Controller_1_Up = 1;
		if (ModernInput_ActionPressed(0, GensInput::Down)) Controller_1_Down = 0;
		else Controller_1_Down = 1;
	}
	
	if (ModernInput_ActionPressed(0, GensInput::Left))
	{
		Controller_1_Left = 0;
		Controller_1_Right = 1;
	}
	else
	{
		Controller_1_Left = 1;
		if (ModernInput_ActionPressed(0, GensInput::Right)) Controller_1_Right = 0;
		else Controller_1_Right = 1;
	}

	if (ModernInput_ActionPressed(0, GensInput::Start)) Controller_1_Start = 0;
	else Controller_1_Start = 1;

	if (ModernInput_ActionPressed(0, GensInput::A)) Controller_1_A = 0;
	else Controller_1_A = 1;

	if (ModernInput_ActionPressed(0, GensInput::B)) Controller_1_B = 0;
	else Controller_1_B = 1;

	if (ModernInput_ActionPressed(0, GensInput::C)) Controller_1_C = 0;
	else Controller_1_C = 1;

	if (Controller_1_Type & 1)
	{
		if (ModernInput_ActionPressed(0, GensInput::Mode)) Controller_1_Mode = 0;
		else Controller_1_Mode = 1;

		if (ModernInput_ActionPressed(0, GensInput::X)) Controller_1_X = 0;
		else Controller_1_X = 1;

		if (ModernInput_ActionPressed(0, GensInput::Y)) Controller_1_Y = 0;
		else Controller_1_Y = 1;

		if (ModernInput_ActionPressed(0, GensInput::Z)) Controller_1_Z = 0;
		else Controller_1_Z = 1;
	}

	if (ModernInput_ActionPressed(1, GensInput::Up))
	{
		Controller_2_Up = 0;
		Controller_2_Down = 1;
	}
	else
	{
		Controller_2_Up = 1;
		if (ModernInput_ActionPressed(1, GensInput::Down)) Controller_2_Down = 0;
		else Controller_2_Down = 1;
	}

	
	if (ModernInput_ActionPressed(1, GensInput::Left))
	{
		Controller_2_Left = 0;
		Controller_2_Right = 1;
	}
	else
	{
		Controller_2_Left = 1;
		if (ModernInput_ActionPressed(1, GensInput::Right)) Controller_2_Right = 0;
		else Controller_2_Right = 1;
	}

	if (ModernInput_ActionPressed(1, GensInput::Start)) Controller_2_Start = 0;
	else Controller_2_Start = 1;

	if (ModernInput_ActionPressed(1, GensInput::A)) Controller_2_A = 0;
	else Controller_2_A = 1;

	if (ModernInput_ActionPressed(1, GensInput::B)) Controller_2_B = 0;
	else Controller_2_B = 1;

	if (ModernInput_ActionPressed(1, GensInput::C)) Controller_2_C = 0;
	else Controller_2_C = 1;

	if (Controller_2_Type & 1)
	{
		if (ModernInput_ActionPressed(1, GensInput::Mode)) Controller_2_Mode = 0;
		else Controller_2_Mode = 1;

		if (ModernInput_ActionPressed(1, GensInput::X)) Controller_2_X = 0;
		else Controller_2_X = 1;

		if (ModernInput_ActionPressed(1, GensInput::Y)) Controller_2_Y = 0;
		else Controller_2_Y = 1;

		if (ModernInput_ActionPressed(1, GensInput::Z)) Controller_2_Z = 0;
		else Controller_2_Z = 1;
	}

	if (Controller_1_Type & 0x10)			// TEAMPLAYER PORT 1
	{
		if (ModernInput_ActionPressed(2, GensInput::Up))
		{
			Controller_1B_Up = 0;
			Controller_1B_Down = 1;
		}
		else
		{
			Controller_1B_Up = 1;
			if (ModernInput_ActionPressed(2, GensInput::Down)) Controller_1B_Down = 0;
			else Controller_1B_Down = 1;
		}
	
		if (ModernInput_ActionPressed(2, GensInput::Left))
		{
			Controller_1B_Left = 0;
			Controller_1B_Right = 1;
		}
		else
		{
			Controller_1B_Left = 1;
			if (ModernInput_ActionPressed(2, GensInput::Right)) Controller_1B_Right = 0;
			else Controller_1B_Right = 1;
		}

		if (ModernInput_ActionPressed(2, GensInput::Start)) Controller_1B_Start = 0;
		else Controller_1B_Start = 1;

		if (ModernInput_ActionPressed(2, GensInput::A)) Controller_1B_A = 0;
		else Controller_1B_A = 1;

		if (ModernInput_ActionPressed(2, GensInput::B)) Controller_1B_B = 0;
		else Controller_1B_B = 1;

		if (ModernInput_ActionPressed(2, GensInput::C)) Controller_1B_C = 0;
		else Controller_1B_C = 1;

		if (Controller_1B_Type & 1)
		{
			if (ModernInput_ActionPressed(2, GensInput::Mode)) Controller_1B_Mode = 0;
			else Controller_1B_Mode = 1;

			if (ModernInput_ActionPressed(2, GensInput::X)) Controller_1B_X = 0;
			else Controller_1B_X = 1;

			if (ModernInput_ActionPressed(2, GensInput::Y)) Controller_1B_Y = 0;
			else Controller_1B_Y = 1;

			if (ModernInput_ActionPressed(2, GensInput::Z)) Controller_1B_Z = 0;
			else Controller_1B_Z = 1;
		}

		if (ModernInput_ActionPressed(3, GensInput::Up))
		{
			Controller_1C_Up = 0;
			Controller_1C_Down = 1;
		}
		else
		{
			Controller_1C_Up = 1;
			if (ModernInput_ActionPressed(3, GensInput::Down)) Controller_1C_Down = 0;
			else Controller_1C_Down = 1;
		}
	
		if (ModernInput_ActionPressed(3, GensInput::Left))
		{
			Controller_1C_Left = 0;
			Controller_1C_Right = 1;
		}
		else
		{
			Controller_1C_Left = 1;
			if (ModernInput_ActionPressed(3, GensInput::Right)) Controller_1C_Right = 0;
			else Controller_1C_Right = 1;
		}

		if (ModernInput_ActionPressed(3, GensInput::Start)) Controller_1C_Start = 0;
		else Controller_1C_Start = 1;

		if (ModernInput_ActionPressed(3, GensInput::A)) Controller_1C_A = 0;
		else Controller_1C_A = 1;

		if (ModernInput_ActionPressed(3, GensInput::B)) Controller_1C_B = 0;
		else Controller_1C_B = 1;

		if (ModernInput_ActionPressed(3, GensInput::C)) Controller_1C_C = 0;
		else Controller_1C_C = 1;

		if (Controller_1C_Type & 1)
		{
			if (ModernInput_ActionPressed(3, GensInput::Mode)) Controller_1C_Mode = 0;
			else Controller_1C_Mode = 1;

			if (ModernInput_ActionPressed(3, GensInput::X)) Controller_1C_X = 0;
			else Controller_1C_X = 1;

			if (ModernInput_ActionPressed(3, GensInput::Y)) Controller_1C_Y = 0;
			else Controller_1C_Y = 1;

			if (ModernInput_ActionPressed(3, GensInput::Z)) Controller_1C_Z = 0;
			else Controller_1C_Z = 1;
		}

		if (ModernInput_ActionPressed(4, GensInput::Up))
		{
			Controller_1D_Up = 0;
			Controller_1D_Down = 1;
		}
		else
		{
			Controller_1D_Up = 1;
			if (ModernInput_ActionPressed(4, GensInput::Down)) Controller_1D_Down = 0;
			else Controller_1D_Down = 1;
		}
	
		if (ModernInput_ActionPressed(4, GensInput::Left))
		{
			Controller_1D_Left = 0;
			Controller_1D_Right = 1;
		}
		else
		{
			Controller_1D_Left = 1;
			if (ModernInput_ActionPressed(4, GensInput::Right)) Controller_1D_Right = 0;
			else Controller_1D_Right = 1;
		}

		if (ModernInput_ActionPressed(4, GensInput::Start)) Controller_1D_Start = 0;
		else Controller_1D_Start = 1;

		if (ModernInput_ActionPressed(4, GensInput::A)) Controller_1D_A = 0;
		else Controller_1D_A = 1;

		if (ModernInput_ActionPressed(4, GensInput::B)) Controller_1D_B = 0;
		else Controller_1D_B = 1;

		if (ModernInput_ActionPressed(4, GensInput::C)) Controller_1D_C = 0;
		else Controller_1D_C = 1;

		if (Controller_1D_Type & 1)
		{
			if (ModernInput_ActionPressed(4, GensInput::Mode)) Controller_1D_Mode = 0;
			else Controller_1D_Mode = 1;

			if (ModernInput_ActionPressed(4, GensInput::X)) Controller_1D_X = 0;
			else Controller_1D_X = 1;

			if (ModernInput_ActionPressed(4, GensInput::Y)) Controller_1D_Y = 0;
			else Controller_1D_Y = 1;

			if (ModernInput_ActionPressed(4, GensInput::Z)) Controller_1D_Z = 0;
			else Controller_1D_Z = 1;
		}
	}

	if (Controller_2_Type & 0x10)			// TEAMPLAYER PORT 2
	{
		if (ModernInput_ActionPressed(5, GensInput::Up))
		{
			Controller_2B_Up = 0;
			Controller_2B_Down = 1;
		}
		else
		{
			Controller_2B_Up = 1;
			if (ModernInput_ActionPressed(5, GensInput::Down)) Controller_2B_Down = 0;
			else Controller_2B_Down = 1;
		}
	
		if (ModernInput_ActionPressed(5, GensInput::Left))
		{
			Controller_2B_Left = 0;
			Controller_2B_Right = 1;
		}
		else
		{
			Controller_2B_Left = 1;
			if (ModernInput_ActionPressed(5, GensInput::Right)) Controller_2B_Right = 0;
			else Controller_2B_Right = 1;
		}

		if (ModernInput_ActionPressed(5, GensInput::Start)) Controller_2B_Start = 0;
		else Controller_2B_Start = 1;

		if (ModernInput_ActionPressed(5, GensInput::A)) Controller_2B_A = 0;
		else Controller_2B_A = 1;

		if (ModernInput_ActionPressed(5, GensInput::B)) Controller_2B_B = 0;
		else Controller_2B_B = 1;

		if (ModernInput_ActionPressed(5, GensInput::C)) Controller_2B_C = 0;
		else Controller_2B_C = 1;

		if (Controller_2B_Type & 1)
		{
			if (ModernInput_ActionPressed(5, GensInput::Mode)) Controller_2B_Mode = 0;
			else Controller_2B_Mode = 1;

			if (ModernInput_ActionPressed(5, GensInput::X)) Controller_2B_X = 0;
			else Controller_2B_X = 1;

			if (ModernInput_ActionPressed(5, GensInput::Y)) Controller_2B_Y = 0;
			else Controller_2B_Y = 1;

			if (ModernInput_ActionPressed(5, GensInput::Z)) Controller_2B_Z = 0;
			else Controller_2B_Z = 1;
		}

		if (ModernInput_ActionPressed(6, GensInput::Up))
		{
			Controller_2C_Up = 0;
			Controller_2C_Down = 1;
		}
		else
		{
			Controller_2C_Up = 1;
			if (ModernInput_ActionPressed(6, GensInput::Down)) Controller_2C_Down = 0;
			else Controller_2C_Down = 1;
		}
	
		if (ModernInput_ActionPressed(6, GensInput::Left))
		{
			Controller_2C_Left = 0;
			Controller_2C_Right = 1;
		}
		else
		{
			Controller_2C_Left = 1;
			if (ModernInput_ActionPressed(6, GensInput::Right)) Controller_2C_Right = 0;
			else Controller_2C_Right = 1;
		}

		if (ModernInput_ActionPressed(6, GensInput::Start)) Controller_2C_Start = 0;
		else Controller_2C_Start = 1;

		if (ModernInput_ActionPressed(6, GensInput::A)) Controller_2C_A = 0;
		else Controller_2C_A = 1;

		if (ModernInput_ActionPressed(6, GensInput::B)) Controller_2C_B = 0;
		else Controller_2C_B = 1;

		if (ModernInput_ActionPressed(6, GensInput::C)) Controller_2C_C = 0;
		else Controller_2C_C = 1;

		if (Controller_2C_Type & 1)
		{
			if (ModernInput_ActionPressed(6, GensInput::Mode)) Controller_2C_Mode = 0;
			else Controller_2C_Mode = 1;

			if (ModernInput_ActionPressed(6, GensInput::X)) Controller_2C_X = 0;
			else Controller_2C_X = 1;

			if (ModernInput_ActionPressed(6, GensInput::Y)) Controller_2C_Y = 0;
			else Controller_2C_Y = 1;

			if (ModernInput_ActionPressed(6, GensInput::Z)) Controller_2C_Z = 0;
			else Controller_2C_Z = 1;
		}

		if (ModernInput_ActionPressed(7, GensInput::Up))
		{
			Controller_2D_Up = 0;
			Controller_2D_Down = 1;
		}
		else
		{
			Controller_2D_Up = 1;
			if (ModernInput_ActionPressed(7, GensInput::Down)) Controller_2D_Down = 0;
			else Controller_2D_Down = 1;
		}
	
		if (ModernInput_ActionPressed(7, GensInput::Left))
		{
			Controller_2D_Left = 0;
			Controller_2D_Right = 1;
		}
		else
		{
			Controller_2D_Left = 1;
			if (ModernInput_ActionPressed(7, GensInput::Right)) Controller_2D_Right = 0;
			else Controller_2D_Right = 1;
		}

		if (ModernInput_ActionPressed(7, GensInput::Start)) Controller_2D_Start = 0;
		else Controller_2D_Start = 1;

		if (ModernInput_ActionPressed(7, GensInput::A)) Controller_2D_A = 0;
		else Controller_2D_A = 1;

		if (ModernInput_ActionPressed(7, GensInput::B)) Controller_2D_B = 0;
		else Controller_2D_B = 1;

		if (ModernInput_ActionPressed(7, GensInput::C)) Controller_2D_C = 0;
		else Controller_2D_C = 1;

		if (Controller_2D_Type & 1)
		{
			if (ModernInput_ActionPressed(7, GensInput::Mode)) Controller_2D_Mode = 0;
			else Controller_2D_Mode = 1;

			if (ModernInput_ActionPressed(7, GensInput::X)) Controller_2D_X = 0;
			else Controller_2D_X = 1;

			if (ModernInput_ActionPressed(7, GensInput::Y)) Controller_2D_Y = 0;
			else Controller_2D_Y = 1;

			if (ModernInput_ActionPressed(7, GensInput::Z)) Controller_2D_Z = 0;
			else Controller_2D_Z = 1;
		}
	}
}


int Setting_Keys(HWND hset)
{
    (void)hset;
	if(!KeyIndex)
		return 0;

	int Checker =Get_Key();

	if(Checker == -1)
		return 0;
	else
	{
		switch(KeyIndex)
		{
		case 1:
			Keys_Def[Player].Up = Checker;
			break;
		case 2:
			Keys_Def[Player].Down = Checker;
			break;
		case 3:
			Keys_Def[Player].Left = Checker;
			break;
		case 4:
			Keys_Def[Player].Right = Checker;
			break;

		case 5:
			Keys_Def[Player].Start = Checker;
			break;
		case 6:
			Keys_Def[Player].Mode = Checker;
			break;

		case 7:
			Keys_Def[Player].A = Checker;
			break;
		case 8:
			Keys_Def[Player].B = Checker;
			break;
		case 9:
			Keys_Def[Player].C = Checker;
			break;

		case 10:
			Keys_Def[Player].X = Checker;
			break;
		case 11:
			Keys_Def[Player].Y = Checker;
			break;
		case 12:
			Keys_Def[Player].Z = Checker;
			break;
		}
		return 1;
	}

}

void Scan_Player_Net(int Player)
{
	if (!Player) return;

	Update_Input();

	if (ModernInput_ActionPressed(0, GensInput::Up))
	{
		Kaillera_Keys[0] &= ~0x08;
		Kaillera_Keys[0] |= 0x04;
	}
	else
	{
		Kaillera_Keys[0] |= 0x08;
		if (ModernInput_ActionPressed(0, GensInput::Down)) Kaillera_Keys[0] &= ~0x04;
		else Kaillera_Keys[0] |= 0x04;
	}
	
	if (ModernInput_ActionPressed(0, GensInput::Left))
	{
		Kaillera_Keys[0] &= ~0x02;
		Kaillera_Keys[0] |= 0x01;
	}
	else
	{
		Kaillera_Keys[0] |= 0x02;
		if (ModernInput_ActionPressed(0, GensInput::Right)) Kaillera_Keys[0] &= ~0x01;
		else Kaillera_Keys[0] |= 0x01;
	}

	if (ModernInput_ActionPressed(0, GensInput::Start)) Kaillera_Keys[0] &= ~0x80;
	else Kaillera_Keys[0] |= 0x80;

	if (ModernInput_ActionPressed(0, GensInput::A)) Kaillera_Keys[0] &= ~0x40;
	else Kaillera_Keys[0] |= 0x40;

	if (ModernInput_ActionPressed(0, GensInput::B)) Kaillera_Keys[0] &= ~0x20;
	else Kaillera_Keys[0] |= 0x20;

	if (ModernInput_ActionPressed(0, GensInput::C)) Kaillera_Keys[0] &= ~0x10;
	else Kaillera_Keys[0] |= 0x10;

	if (Controller_1_Type & 1)
	{
		if (ModernInput_ActionPressed(0, GensInput::Mode)) Kaillera_Keys[1] &= ~0x08;
		else Kaillera_Keys[1] |= 0x08;

		if (ModernInput_ActionPressed(0, GensInput::X)) Kaillera_Keys[1] &= ~0x04;
		else Kaillera_Keys[1] |= 0x04;

		if (ModernInput_ActionPressed(0, GensInput::Y)) Kaillera_Keys[1] &= ~0x02;
		else Kaillera_Keys[1] |= 0x02;

		if (ModernInput_ActionPressed(0, GensInput::Z)) Kaillera_Keys[1] &= ~0x01;
		else Kaillera_Keys[1] |= 0x01;
	}
}


void Update_Controllers_Net(int num_player)
{
	Controller_1_Up = (Kaillera_Keys[0] & 0x08) >> 3;
	Controller_1_Down = (Kaillera_Keys[0] & 0x04) >> 2;
	Controller_1_Left = (Kaillera_Keys[0] & 0x02) >> 1;
	Controller_1_Right = (Kaillera_Keys[0] & 0x01);
	Controller_1_Start = (Kaillera_Keys[0] & 0x80) >> 7;
	Controller_1_A = (Kaillera_Keys[0] & 0x40) >> 6;
	Controller_1_B = (Kaillera_Keys[0] & 0x20) >> 5;
	Controller_1_C = (Kaillera_Keys[0] & 0x10) >> 4;

	if (Controller_1_Type & 1)
	{
		Controller_1_Mode = (Kaillera_Keys[0 + 1] & 0x08) >> 3;
		Controller_1_X = (Kaillera_Keys[0 + 1] & 0x04) >> 2;
		Controller_1_Y = (Kaillera_Keys[0 + 1] & 0x02) >> 1;
		Controller_1_Z = (Kaillera_Keys[0 + 1] & 0x01);
	}

	if (num_player > 2)			// TEAMPLAYER
	{
		Controller_1B_Up = (Kaillera_Keys[2] & 0x08) >> 3;
		Controller_1B_Down = (Kaillera_Keys[2] & 0x04) >> 2;
		Controller_1B_Left = (Kaillera_Keys[2] & 0x02) >> 1;
		Controller_1B_Right = (Kaillera_Keys[2] & 0x01);
		Controller_1B_Start = (Kaillera_Keys[2] & 0x80) >> 7;
		Controller_1B_A = (Kaillera_Keys[2] & 0x40) >> 6;
		Controller_1B_B = (Kaillera_Keys[2] & 0x20) >> 5;
		Controller_1B_C = (Kaillera_Keys[2] & 0x10) >> 4;

		if (Controller_1B_Type & 1)
		{
			Controller_1B_Mode = (Kaillera_Keys[2 + 1] & 0x08) >> 3;
			Controller_1B_X = (Kaillera_Keys[2 + 1] & 0x04) >> 2;
			Controller_1B_Y = (Kaillera_Keys[2 + 1] & 0x02) >> 1;
			Controller_1B_Z = (Kaillera_Keys[2 + 1] & 0x01);
		}

		Controller_1C_Up = (Kaillera_Keys[4] & 0x08) >> 3;
		Controller_1C_Down = (Kaillera_Keys[4] & 0x04) >> 2;
		Controller_1C_Left = (Kaillera_Keys[4] & 0x02) >> 1;
		Controller_1C_Right = (Kaillera_Keys[4] & 0x01);
		Controller_1C_Start = (Kaillera_Keys[4] & 0x80) >> 7;
		Controller_1C_A = (Kaillera_Keys[4] & 0x40) >> 6;
		Controller_1C_B = (Kaillera_Keys[4] & 0x20) >> 5;
		Controller_1C_C = (Kaillera_Keys[4] & 0x10) >> 4;

		if (Controller_1C_Type & 1)
		{
			Controller_1C_Mode = (Kaillera_Keys[4 + 1] & 0x08) >> 3;
			Controller_1C_X = (Kaillera_Keys[4 + 1] & 0x04) >> 2;
			Controller_1C_Y = (Kaillera_Keys[4 + 1] & 0x02) >> 1;
			Controller_1C_Z = (Kaillera_Keys[4 + 1] & 0x01);
		}

		Controller_1D_Up = (Kaillera_Keys[6] & 0x08) >> 3;
		Controller_1D_Down = (Kaillera_Keys[6] & 0x04) >> 2;
		Controller_1D_Left = (Kaillera_Keys[6] & 0x02) >> 1;
		Controller_1D_Right = (Kaillera_Keys[6] & 0x01);
		Controller_1D_Start = (Kaillera_Keys[6] & 0x80) >> 7;
		Controller_1D_A = (Kaillera_Keys[6] & 0x40) >> 6;
		Controller_1D_B = (Kaillera_Keys[6] & 0x20) >> 5;
		Controller_1D_C = (Kaillera_Keys[6] & 0x10) >> 4;

		if (Controller_1D_Type & 1)
		{
			Controller_1D_Mode = (Kaillera_Keys[6 + 1] & 0x08) >> 3;
			Controller_1D_X = (Kaillera_Keys[6 + 1] & 0x04) >> 2;
			Controller_1D_Y = (Kaillera_Keys[6 + 1] & 0x02) >> 1;
			Controller_1D_Z = (Kaillera_Keys[6 + 1] & 0x01);
		}
	}
	else
	{
		Controller_2_Up = (Kaillera_Keys[2] & 0x08) >> 3;
		Controller_2_Down = (Kaillera_Keys[2] & 0x04) >> 2;
		Controller_2_Left = (Kaillera_Keys[2] & 0x02) >> 1;
		Controller_2_Right = (Kaillera_Keys[2] & 0x01);
		Controller_2_Start = (Kaillera_Keys[2] & 0x80) >> 7;
		Controller_2_A = (Kaillera_Keys[2] & 0x40) >> 6;
		Controller_2_B = (Kaillera_Keys[2] & 0x20) >> 5;
		Controller_2_C = (Kaillera_Keys[2] & 0x10) >> 4;

		if (Controller_2_Type & 1)
		{
			Controller_2_Mode = (Kaillera_Keys[2 + 1] & 0x08) >> 3;
			Controller_2_X = (Kaillera_Keys[2 + 1] & 0x04) >> 2;
			Controller_2_Y = (Kaillera_Keys[2 + 1] & 0x02) >> 1;
			Controller_2_Z = (Kaillera_Keys[2 + 1] & 0x01);
		}
	}
}


