/* Gens KO: modified 2026-11-20. See CHANGELOG_KO.txt and original license notices below. */
#ifndef G_INPUT_H
#define G_INPUT_H

#define DIRECTINPUT_VERSION 0x0500  // for joystick support

#include <dinput.h>
//#include <mmsystem.h>

struct K_Def {
	unsigned int Start, Mode;
	unsigned int A, B, C;
	unsigned int X, Y, Z;
	unsigned int Up, Down, Left, Right;
	};

extern struct K_Def Keys_Def[8];
extern unsigned char Kaillera_Keys[16];

extern int KeyIndex;
extern int Player;

int String_Size(char *Chaine);
unsigned int Get_Key(void);
int Check_Key_Pressed(unsigned int key);
int Setting_Keys(HWND hset);
int Init_Input(HINSTANCE hInst, HWND hWnd);
void End_Input(void);
void Update_Input(void);
void Update_Controllers(void);
void Scan_Player_Net(int Player);
void Update_Controllers_Net(int num_player);


// Added 2026-11-20: modern input bridge; GPL v2.
void Input_ClearState(void);
int Input_HasFocus(void);
HWND Input_GetWindow(void);
void Input_ResetJoystickPreferences(void);
void Input_RefreshDevices(void);
int Input_JoystickConnected(int slot);
int Input_JoystickKnown(int slot);
void Input_JoystickName(int slot, wchar_t *out, unsigned capacity);
void Input_JoystickGuid(int slot, char *out, unsigned capacity);
void Input_SetJoystickGuid(int slot, const char *guid);
unsigned Input_FirstPressedCode(int filter);

#endif