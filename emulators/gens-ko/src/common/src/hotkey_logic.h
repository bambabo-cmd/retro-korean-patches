/* Gens KO transport input rules, 2026-11-20. GPL v2; see LICENSE.txt. */
#ifndef GENS_HOTKEY_LOGIC_H
#define GENS_HOTKEY_LOGIC_H
#include "modern_input_logic.h"

namespace GensHotkeys {
enum Mode { Hold=0, Toggle=1 };
enum Modifier { Control=1, Shift=2, Alt=4 };
struct Binding { unsigned key, modifiers, pad; Mode mode; };

inline int Clamp(int value,int minimum,int maximum) {
    return value<minimum?minimum:value>maximum?maximum:value;
}
inline bool ModifierKey(unsigned key) {
    return key==0x1d || key==0x9d || key==0x2a || key==0x36 || key==0x38 || key==0xb8;
}
inline bool ValidKeyboard(unsigned key,unsigned modifiers) {
    if(modifiers>7 || key>=256) return false;
    if(!key) return modifiers==0;
    /* Esc and bare modifiers remain available to the menu and chord editor. */
    if(key==0x01 || key==0xdb || key==0xdc || key==0xdd || ModifierKey(key)) return false;
    if((modifiers&Alt) && (key==0x1c || key==0x9c || key==0x3e)) return false;
    return true;
}
inline bool ValidPad(unsigned pad) { return !pad || (pad>=256 && GensInput::ValidCode(pad)); }
inline bool SameKey(const Binding &a,const Binding &b) {
    return a.key && a.key==b.key && a.modifiers==b.modifiers;
}
inline bool Conflict(const Binding &a,const Binding &b) {
    return SameKey(a,b) || (a.pad && a.pad==b.pad);
}
inline bool Match(const Binding &binding,unsigned key,unsigned modifiers) {
    return binding.key && binding.key==key && binding.modifiers==modifiers;
}
/* One physical rising edge toggles once. Keyboard and pad can coexist without
 * toggling twice while either source remains down. Pad inputs are disarmed
 * until released after focus loss/dialog entry, so reconnects cannot latch FF. */
struct State {
    bool keyboard=false, pad=false, active=false, padNeedsRelease=true;
    void Clear() { keyboard=pad=active=false;padNeedsRelease=true; }
    void SetKeyboard(bool down,Mode mode,bool repeated=false) {
        if(down && repeated) return;
        bool before=keyboard||pad;keyboard=down;Update(before,mode);
    }
    void SetPad(bool down,Mode mode) {
        if(padNeedsRelease) { if(down) return;padNeedsRelease=false; }
        bool before=keyboard||pad;pad=down;Update(before,mode);
    }
    void Update(bool before,Mode mode) {
        bool down=keyboard||pad;
        if(mode==Hold) active=down;
        else if(down && !before) active=!active;
    }
};
}
#endif
