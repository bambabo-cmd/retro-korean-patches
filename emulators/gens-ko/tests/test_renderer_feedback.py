#!/usr/bin/env python3
"""Exercise the actual renderer feedback state functions with a native GL stub.

The function bodies are extracted unchanged from renderer_gl.cpp. This covers
bank lifetime and deferred clear decisions, not Windows/WGL drawing or shaders.
Shader effects are covered separately by test_real_crt_phosphor.py.
SPDX-License-Identifier: GPL-2.0-or-later
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'src/common/src/renderer_gl.cpp'
OUT = ROOT / 'build/renderer_feedback'
OUT.mkdir(parents=True, exist_ok=True)


def extract(source, name):
    marker = 'static void ' + name + '() {'
    start = source.index(marker)
    opening = source.index('{', start)
    depth = 0
    for end in range(opening, len(source)):
        depth += source[end] == '{'
        depth -= source[end] == '}'
        if not depth:
            return source[start:end + 1]
    raise AssertionError('Unclosed function: ' + name)


PREFIX = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
typedef unsigned int GLuint;
static const int GL_FRAMEBUFFER = 0x8d40, GL_COLOR_BUFFER_BIT = 0x4000;
struct Pass { GLuint texture, fbo, historyTexture, historyFbo; };
static std::vector<Pass> passes;
static int feedbackPass = -1;
static uint64_t submittedFrameSerial = 0, feedbackFrameSerial = 0;
static bool feedbackHistoryReady = false, feedbackResetPending = true;
static int checks = 0;
static void check(bool condition) { ++checks; assert(condition); }
static unsigned boundFbo = 0;
static float currentClear[4] = {0, 0, 0, 1};
struct ClearCall { GLuint fbo; float rgba[4]; };
static std::vector<ClearCall> clears;
struct GLStub {
    void BindFramebuffer(int target, GLuint fbo) {
        check(target == GL_FRAMEBUFFER); boundFbo = fbo;
    }
} glx;
static void glClearColor(float r, float g, float b, float a) {
    currentClear[0]=r; currentClear[1]=g; currentClear[2]=b; currentClear[3]=a;
}
static void glClear(int bits) {
    check(bits == GL_COLOR_BUFFER_BIT);
    ClearCall call; call.fbo=boundFbo;
    std::copy(currentClear, currentClear + 4, call.rgba); clears.push_back(call);
}
'''

SUFFIX = r'''
static void expectBanks(unsigned write, unsigned read, unsigned wfbo, unsigned rfbo) {
    const Pass &p=passes[feedbackPass];
    check(p.texture==write && p.historyTexture==read);
    check(p.fbo==wfbo && p.historyFbo==rfbo);
}
static void expectClear(size_t count, unsigned first, unsigned second) {
    check(clears.size()==count);
    check(clears[count-2].fbo==first && clears[count-1].fbo==second);
    for(size_t i=count-2;i<count;++i)for(int c=0;c<4;++c)check(clears[i].rgba[c]==0);
    check(boundFbo==0 && currentClear[3]==1);
    check(!feedbackHistoryReady && !feedbackResetPending);
}
int main() {
    for(unsigned i=0;i<15;++i)passes.push_back(Pass{100+i,200+i,300+i,400+i});
    feedbackPass=6; submittedFrameSerial=10;
    // Initial draw: both banks start black, then successful completion commits.
    resetFeedbackHistory(); check(clears.empty());
    beginFeedbackFrame(); expectBanks(106,306,206,406); expectClear(2,206,406);
    completeFeedbackFrame(); check(feedbackHistoryReady && feedbackFrameSerial==10);
    // Repeated WM_PAINT / menu overlays must not decay afterglow again.
    for(int i=0;i<20;++i) {
        beginFeedbackFrame(); expectBanks(106,306,206,406);
        completeFeedbackFrame(); check(feedbackFrameSerial==10 && clears.size()==2);
    }
    // A submitted frame rotates texture AND framebuffer IDs exactly once.
    submittedFrameSerial=11; beginFeedbackFrame(); expectBanks(306,106,406,206);
    completeFeedbackFrame(); check(feedbackFrameSerial==11);
    beginFeedbackFrame(); expectBanks(306,106,406,206); completeFeedbackFrame();
    // FF can skip serials: one rendered frame still consumes one history bank.
    submittedFrameSerial=500; beginFeedbackFrame(); expectBanks(106,306,206,406);
    completeFeedbackFrame(); check(feedbackFrameSerial==500 && clears.size()==2);
    check(passes[5].texture==105 && passes[7].texture==107);
    // Resize / settings changes defer GL clears until context is current.
    resetFeedbackHistory(); check(feedbackResetPending && !feedbackHistoryReady);
    check(feedbackFrameSerial==0 && clears.size()==2);
    beginFeedbackFrame(); expectBanks(106,306,206,406); expectClear(4,206,406);
    completeFeedbackFrame(); check(feedbackFrameSerial==500);
    // New context/targets must clear the replacement IDs, not stale targets.
    resetFeedbackHistory(); passes[6]=Pass{700,800,900,1000};
    beginFeedbackFrame(); expectBanks(700,900,800,1000); expectClear(6,800,1000);
    completeFeedbackFrame();
    // A target-preparation failure resets without beginning a draw; recovery
    // can happen at the same submitted serial with both banks cleared again.
    resetFeedbackHistory(); check(clears.size()==6);
    beginFeedbackFrame(); expectBanks(700,900,800,1000); expectClear(8,800,1000);
    completeFeedbackFrame();
    // Disable feedback; no pass is indexed and no GL buffer is cleared.
    feedbackPass=-1; resetFeedbackHistory(); beginFeedbackFrame(); completeFeedbackFrame();
    check(clears.size()==8 && !feedbackHistoryReady && !feedbackResetPending);
    // GBA CRT has a different feedback pass. Only its banks rotate.
    feedbackPass=7; resetFeedbackHistory();
    beginFeedbackFrame(); expectBanks(107,307,207,407); expectClear(10,207,407);
    completeFeedbackFrame(); ++submittedFrameSerial;
    beginFeedbackFrame(); expectBanks(307,107,407,207); completeFeedbackFrame();
    check(passes[6].texture==700 && clears.size()==10);
    std::printf("%d assertions passed\n", checks);
}
'''


def main():
    source = SOURCE.read_text(encoding='utf-8')
    names = ['resetFeedbackHistory', 'clearFeedbackTargets', 'beginFeedbackFrame', 'completeFeedbackFrame']
    functions = '\n\n'.join(extract(source, name) for name in names)
    harness = OUT / 'renderer_feedback.cpp'
    harness.write_text(PREFIX + functions + SUFFIX)
    exe = OUT / 'renderer_feedback'
    command = [os.environ.get('CXX', 'g++'), '-std=c++11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=address,undefined', '-fno-omit-frame-pointer', str(harness), '-o', str(exe)]
    subprocess.run(command, check=True)
    # The managed runner uses ptrace; LSan cannot enumerate threads there.
    # ASan/UBSan remain enabled. Leak detection is not claimed by this test.
    result = subprocess.run([str(exe)], capture_output=True, text=True,
                            env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    report = {
        'status': 'pass',
        'scope': 'Unchanged renderer_gl.cpp feedback state functions with native GL call stub; not WGL/Windows execution.',
        'functions': names,
        'sanitizers': ['address', 'undefined'],
        'leak_detection': 'disabled: LeakSanitizer is incompatible with runner ptrace',
        'assertions': int(result.stdout.split()[0]),
        'scenarios': ['initial transparent clear of both banks', 'same-frame repaint/overlay stability',
                      'new frame texture and framebuffer swap', 'skipped frame serials',
                      'deferred resize/settings reset', 'context/target replacement',
                      'target preparation failure recovery', 'disabled feedback', 'GBA feedback pass'],
        'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in [SOURCE, Path(__file__)]},
        'extracted_functions_sha256': hashlib.sha256(functions.encode()).hexdigest(),
        'harness_sha256': hashlib.sha256(harness.read_bytes()).hexdigest(),
        'compile_command': command,
    }
    (OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(result.stdout.strip())
    print(OUT / 'report.json')


if __name__ == '__main__':
    main()
