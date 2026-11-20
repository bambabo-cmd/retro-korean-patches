/* Gens KO input extension, 2026-11-20. GPL v2; see LICENSE.txt. */
#ifndef GENS_MODERN_INPUT_LOGIC_H
#define GENS_MODERN_INPUT_LOGIC_H

/* Pure input rules, shared by the Win32 implementation and host regression tests. */
namespace GensInput {
enum Action { Start, Mode, A, B, C, X, Y, Z, Up, Down, Left, Right, ActionCount };
enum XControl {
    XUp=1, XDown, XLeft, XRight, XStart, XBack, XLeftThumb, XRightThumb,
    XLeftShoulder, XRightShoulder, XA, XB, XX, XY, XLeftTrigger, XRightTrigger,
    XLeftStickLeft, XLeftStickRight, XLeftStickUp, XLeftStickDown,
    XRightStickLeft, XRightStickRight, XRightStickUp, XRightStickDown
};

/* DirectInput scan codes are stable across keyboard layouts. This table is
 * shared with tests so the r2 default/migration checks cover the actual UI. */
inline unsigned KeyboardDefaultCode(int player,int action) {
    static const unsigned codes[2][ActionCount]={
        {0x2f,0x21,0x2e,0x2d,0x2c,0x20,0x1f,0x1e,0xc8,0xd0,0xcb,0xcd},
        {0x16,0x14,0x25,0x26,0x32,0x17,0x18,0x19,0x15,0x23,0x22,0x24}
    };
    return player>=0 && player<2 && action>=0 && action<ActionCount?codes[player][action]:0;
}
inline bool IsLegacyP1Default(const unsigned *codes) {
    static const unsigned legacy[ActionCount]={0x1c,0x36,0x1e,0x1f,0x20,0x2c,0x2d,0x2e,0xc8,0xd0,0xcb,0xcd};
    if(!codes) return false;
    for(int a=0;a<ActionCount;++a) if(codes[a]!=legacy[a]) return false;
    return true;
}

inline bool ValidCode(unsigned code) {
    if (code < 256) return true; /* zero means unbound */
    unsigned family=code & 0xF000, slot=(code>>8)&15, value=code&255;
    if (code & ~0xFFFFu) return false;
    if (family == 0x2000) return slot < 4 && value >= XUp && value <= XRightStickDown;
    if (family != 0x1000 || slot >= 8) return false;
    if (value >= 1 && value <= 4) return true; /* legacy X/Y axes */
    if (value >= 0x10 && value < 0x30) return true; /* 32 buttons */
    if (value >= 0x40 && value < 0x50) return true; /* eight +/- axes */
    return (value & 0xC0) == 0x80 && (value & 15) >= 1 && (value & 15) <= 4;
}

inline bool PovPressed(unsigned angle, unsigned direction) {
    /* DIJOYSTATE uses 0xFFFFFFFF (sometimes 0xFFFF) for a centered hat. */
    if ((angle & 0xFFFF) == 0xFFFF || angle >= 36000) return false;
    unsigned octant=((angle + 2250) % 36000) / 4500;
    switch (direction) {
    case 1: return octant==7 || octant==0 || octant==1;
    case 2: return octant==1 || octant==2 || octant==3;
    case 3: return octant==3 || octant==4 || octant==5;
    case 4: return octant==5 || octant==6 || octant==7;
    default: return false;
    }
}

inline int ClampDeadzone(int percent) { return percent < 10 ? 10 : percent > 60 ? 60 : percent; }
inline bool AxisPressed(long value, bool positive, long range, int percent) {
    long limit=range * ClampDeadzone(percent) / 100;
    return positive ? value > limit : value < -limit;
}

inline bool XPressed(unsigned value, unsigned buttons, unsigned leftTrigger,
                     unsigned rightTrigger, int lx, int ly, int rx, int ry,
                     int percent) {
    static const unsigned masks[14]={
        0x0001,0x0002,0x0004,0x0008,0x0010,0x0020,0x0040,0x0080,
        0x0100,0x0200,0x1000,0x2000,0x4000,0x8000
    };
    if (value>=1 && value<=14) return (buttons & masks[value-1])!=0;
    if (value==XLeftTrigger) return leftTrigger>30;
    if (value==XRightTrigger) return rightTrigger>30;
    switch(value) {
    case XLeftStickLeft: return AxisPressed(lx,false,32767,percent);
    case XLeftStickRight:return AxisPressed(lx,true,32767,percent);
    case XLeftStickUp:return AxisPressed(ly,true,32767,percent);
    case XLeftStickDown:return AxisPressed(ly,false,32767,percent);
    case XRightStickLeft:return AxisPressed(rx,false,32767,percent);
    case XRightStickRight:return AxisPressed(rx,true,32767,percent);
    case XRightStickUp:return AxisPressed(ry,true,32767,percent);
    case XRightStickDown:return AxisPressed(ry,false,32767,percent);
    default:return false;
    }
}

inline unsigned XDefault(int action, unsigned slot) {
    static const unsigned map[ActionCount]={XStart,XBack,XX,XA,XB,
        XLeftShoulder,XY,XRightShoulder,XUp,XDown,XLeft,XRight};
    return action>=0 && action<ActionCount && slot<4 ? 0x2000|(slot<<8)|map[action] : 0;
}

inline unsigned StickAlias(unsigned code, bool enabled) {
    if(!enabled || !ValidCode(code) || (code&0xF000)!=0x2000) return 0;
    unsigned value=code&255;
    if(value<1 || value>4) return 0;
    static const unsigned axis[5]={0,XLeftStickUp,XLeftStickDown,XLeftStickLeft,XLeftStickRight};
    return (code&0xFF00)|axis[value];
}
}
#endif
