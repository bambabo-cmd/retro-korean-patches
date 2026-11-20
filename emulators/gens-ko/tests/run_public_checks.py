#!/usr/bin/env python3
"""Tests for the public, core-free contribution subset."""
from pathlib import Path
import os,subprocess,tempfile

ROOT=Path(__file__).resolve().parents[1]
environment=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
with tempfile.TemporaryDirectory(prefix='gens-public-checks-') as temporary:
    for name in ('test_hotkeys','test_modern_input','test_playback_clock','test_rewind_history'):
        binary=Path(temporary)/name
        command=['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                 '-fno-omit-frame-pointer','-I'+str(ROOT/'src/common/src'),
                 str(ROOT/'tests'/(name+'.cpp')),'-lz','-o',str(binary)]
        subprocess.run(command,env=environment,check=True)
        subprocess.run([str(binary)],env=environment,check=True)
