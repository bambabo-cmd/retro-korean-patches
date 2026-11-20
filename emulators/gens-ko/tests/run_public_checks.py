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
    binary=Path(temporary)/'test_shader_preset'
    subprocess.run(['g++','-std=c++17','-O1','-g','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',str(ROOT/'tests/test_shader_preset.cpp'),
                    str(ROOT/'src/common/src/shader_preset.cpp'),'-o',str(binary)],env=environment,check=True)
    subprocess.run([str(binary),*[str(p) for p in sorted((ROOT/'src/shaders').rglob('*.glslp'))]],env=environment,check=True)
subprocess.run(['python3',str(ROOT/'port_real_crt_phosphor.py'),'--check'],env=environment,check=True)
