/* Gens KO r2 input state regressions (GPL v2). */
#include "../src/common/src/hotkey_logic.h"
#include <assert.h>
#include <stdio.h>
using namespace GensHotkeys;
static unsigned checks=0;
#define CHECK(expression) do { assert(expression);++checks; } while(0)
int main() {
    State s;
    // Hold follows both edges. Key auto-repeat must never toggle or restart it.
    s.SetKeyboard(true,Hold);CHECK(s.active);
    for(int n=0;n<100;++n) { s.SetKeyboard(true,Hold,true);CHECK(s.active); }
    s.SetKeyboard(false,Hold);CHECK(!s.active);
    // A complete quick tap between two playback polls remains latched once.
    s.SetKeyboard(true,Toggle);s.SetKeyboard(false,Toggle);CHECK(s.active);
    for(int n=0;n<100;++n) { s.SetPad(false,Toggle);CHECK(s.active); }
    s.SetKeyboard(true,Toggle);CHECK(!s.active);
    for(int n=0;n<100;++n) { s.SetKeyboard(true,Toggle,true);CHECK(!s.active); }
    s.SetKeyboard(false,Toggle);CHECK(!s.active);
    // Focus loss cancels toggles and waits for held pad buttons to be released.
    s.SetPad(true,Toggle);CHECK(s.active);
    s.Clear();CHECK(!s.active);
    s.SetPad(true,Toggle);CHECK(!s.active);
    s.SetKeyboard(true,Toggle,true);CHECK(!s.active);
    s.SetPad(false,Toggle);CHECK(!s.active);
    s.SetPad(true,Toggle);CHECK(s.active);
    s.SetPad(false,Toggle);CHECK(s.active);
    // Simultaneous keyboard + pad sources are one combined physical press.
    s.Clear();s.SetPad(false,Toggle);s.SetKeyboard(true,Toggle);CHECK(s.active);
    s.SetPad(true,Toggle);CHECK(s.active);
    s.SetKeyboard(false,Toggle);CHECK(s.active);
    s.SetPad(false,Toggle);CHECK(s.active);
    s.SetPad(true,Toggle);CHECK(!s.active);
    s.Clear();s.SetPad(false,Hold);s.SetKeyboard(true,Hold);s.SetPad(true,Hold);
    s.SetKeyboard(false,Hold);CHECK(s.active);s.SetPad(false,Hold);CHECK(!s.active);
    // Modifier matching protects Shift+Backspace screenshots from plain REW.
    Binding rew={0x0e,0,0,Hold},shot={0x0e,Shift,0,Hold},copy=rew;
    CHECK(Match(rew,0x0e,0));CHECK(!Match(rew,0x0e,Shift));CHECK(Match(shot,0x0e,Shift));
    CHECK(!Conflict(rew,shot));CHECK(Conflict(rew,copy));
    rew.pad=copy.pad=0x2005;copy.key=0;CHECK(Conflict(rew,copy));
    copy.pad=0x2105;CHECK(!Conflict(rew,copy));
    CHECK(ValidKeyboard(0x0f,0));CHECK(ValidKeyboard(0x39,0));CHECK(ValidKeyboard(0x21,Control|Shift));
    CHECK(!ValidKeyboard(0x01,0));CHECK(!ValidKeyboard(0x38,0));CHECK(!ValidKeyboard(0xdb,0));
    CHECK(!ValidKeyboard(0x1c,Alt));CHECK(!ValidKeyboard(0x3e,Alt));CHECK(!ValidKeyboard(0x2000,0));
    CHECK(!ValidKeyboard(0,Control));CHECK(ValidKeyboard(0,0));CHECK(!ValidKeyboard(0x21,8));
    CHECK(ValidPad(0));CHECK(ValidPad(0x2005));CHECK(!ValidPad(0x2401));CHECK(!ValidPad(0x39));
    // Untrusted INI values have bounded CPU rates and bounded rewind memory.
    CHECK(Clamp(-500,10,1000)==10);CHECK(Clamp(2147483647,32,512)==512);
    CHECK(Clamp(400,100,1600)==400);CHECK(Clamp(5,5,100)==5);
    // Default game mapping and exact legacy migration, without modifying a
    // user's layout after even one deliberate edit.
    using namespace GensInput;
    const unsigned requested[ActionCount]={0x2f,0x21,0x2e,0x2d,0x2c,0x20,0x1f,0x1e,0xc8,0xd0,0xcb,0xcd};
    unsigned legacy[ActionCount]={0x1c,0x36,0x1e,0x1f,0x20,0x2c,0x2d,0x2e,0xc8,0xd0,0xcb,0xcd};
    CHECK(IsLegacyP1Default(legacy));
    for(int i=0;i<ActionCount;++i) {
        CHECK(KeyboardDefaultCode(0,i)==requested[i]);
        unsigned old=legacy[i];legacy[i]=0;CHECK(!IsLegacyP1Default(legacy));legacy[i]=old;
        for(int j=0;j<i;++j) CHECK(KeyboardDefaultCode(0,j)!=KeyboardDefaultCode(0,i));
    }
    CHECK(!IsLegacyP1Default(requested));CHECK(!IsLegacyP1Default(NULL));
    CHECK(!KeyboardDefaultCode(2,Start));CHECK(!KeyboardDefaultCode(-1,A));
    printf("PASS: %u transport input assertions (hold/toggle, quick taps, repeats, focus, dual input, modifiers, defaults, migration)\n",checks);
}
