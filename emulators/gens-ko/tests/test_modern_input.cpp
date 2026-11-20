// Behavioral regressions for the Windows input extension (GPL v2).
#include "../src/common/src/modern_input_logic.h"
#include <assert.h>
#include <stdio.h>
using namespace GensInput;
int main() {
    unsigned count=0;
    // A centered DirectInput hat must not become an Up press after a failed poll.
    assert(!PovPressed(0xFFFFFFFFu,1));++count;
    assert(!PovPressed(0xFFFFu,1));++count;
    // Each cardinal direction has one output, each diagonal has both neighbors.
    const unsigned expected[8]={1,3,2,6,4,12,8,9};
    for(unsigned oct=0;oct<8;++oct) {
        unsigned actual=0;
        for(unsigned d=1;d<=4;++d) if(PovPressed(oct*4500,d)) actual|=1u<<(d-1);
        assert(actual==expected[oct]);++count;
    }
    // Reject old malformed cfg values that could index past Joy_ID or rgbButtons.
    const unsigned invalid[]={0x800,0x1801,0x1F10,0x1030,0x107F,0x10C1,0x2000,0x2401,0x20FF,0xFFFFFFFFu};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) { assert(!ValidCode(invalid[i]));++count; }
    for(unsigned code=0;code<65536;++code) if(ValidCode(code) && code>=256) {
        unsigned slot=(code>>8)&15;
        assert(((code&0xF000)==0x1000 && slot<8) || ((code&0xF000)==0x2000 && slot<4));++count;
    }
    // All 32 DirectInput buttons must retain separate codes, including 17..32.
    for(unsigned slot=0;slot<8;++slot) for(unsigned button=0;button<32;++button) {
        unsigned code=0x1000|(slot<<8)|(0x10+button);
        assert(ValidCode(code));assert((code&255)-0x10==button);++count;
    }
    // Noise within the configured deadzone stays idle; signs and boundary matter.
    assert(!AxisPressed(250,true,1000,25));++count;
    assert(AxisPressed(251,true,1000,25));++count;
    assert(!AxisPressed(-250,false,1000,25));++count;
    assert(AxisPressed(-251,false,1000,25));++count;
    assert(!AxisPressed(200,false,1000,25));++count;
    assert(!AxisPressed(-200,true,1000,25));++count;
    // Independent XInput triggers (no shared-axis cancellation), click and axes.
    assert(XPressed(XLeftTrigger,0,200,200,0,0,0,0,25));++count;
    assert(XPressed(XRightTrigger,0,200,200,0,0,0,0,25));++count;
    assert(!XPressed(XLeftTrigger,0,30,0,0,0,0,0,25));++count;
    assert(XPressed(XLeftThumb,0x40,0,0,0,0,0,0,25));++count;
    assert(!XPressed(XRightThumb,0x40,0,0,0,0,0,0,25));++count;
    assert(XPressed(XLeftStickUp,0,0,0,0,32767,0,0,25));++count;
    assert(!XPressed(XLeftStickDown,0,0,0,0,32767,0,0,25));++count;
    // Default player layouts must be valid, isolated per slot and conflict free.
    for(unsigned slot=0;slot<4;++slot) for(int a=0;a<ActionCount;++a) {
        unsigned code=XDefault(a,slot);assert(ValidCode(code));assert(((code>>8)&15)==slot);
        for(int b=0;b<a;++b) assert(XDefault(b,slot)!=code);
        ++count;
    }
    for(unsigned slot=0;slot<4;++slot) {
        assert(StickAlias(XDefault(Up,slot),true)==(0x2000u|(slot<<8)|XLeftStickUp));++count;
        assert(!StickAlias(XDefault(Up,slot),false));++count;
    }
    assert(!StickAlias(0x1081,true));++count;
    assert(!StickAlias(0x2801,true));++count;
    printf("PASS: %u input assertions (POV diagonals, safe bindings, 32 buttons, deadzone, XInput and defaults)\n",count);
}
