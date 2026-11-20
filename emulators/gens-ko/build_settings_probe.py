#!/usr/bin/env python3
"""Cross-build an actual Gens KO settings UI component without emulation cores.

GPL-2.0, 2026-11-20. The exact production input/dialog source files are compiled.
Only controller exported storage and Make_IO_Table are supplied by the harness.
This target has no Starscream, Kaillera, Microsoft wave sample, mpglib or ROM.
"""
from pathlib import Path
import hashlib,json,os,re,shutil,subprocess

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'build'/'settings_probe'
STAGE=OUT/'stage'
for directory in (OUT,STAGE):directory.mkdir(parents=True,exist_ok=True)
environment=os.environ.copy()
sysroot=ROOT/'toolchain'/'sysroot'
if sysroot.exists():
    environment['PATH']=str(sysroot/'usr/bin')+os.pathsep+environment.get('PATH','')
    environment['LD_LIBRARY_PATH']=str(sysroot/'usr/lib/x86_64-linux-gnu')+os.pathsep+environment.get('LD_LIBRARY_PATH','')
def tool(name):
    return shutil.which(name,path=environment.get('PATH')) or shutil.which(name.removesuffix('-posix'),path=environment.get('PATH')) or name
compiler=tool('i686-w64-mingw32-g++-posix')
ccompiler=tool('i686-w64-mingw32-gcc-posix')
resourceCompiler=tool('i686-w64-mingw32-windres')
objdump=tool('i686-w64-mingw32-objdump')

production=[
    'src/common/src/modern_input.cpp','src/common/src/modern_input.h','src/common/src/modern_input_logic.h',
    'src/common/src/hotkeys.cpp','src/common/src/hotkeys.h','src/common/src/hotkey_logic.h',
    'src/common/src/G_dinput.cpp','src/common/src/G_Input.h','src/common/src/G_main.h','src/common/src/io.h',
    'src/win32/src/resource.h','src/common/src/ko_manifest.xml',
]
inputs=[ROOT/name for name in production]+[ROOT/'tests/windows_settings_probe.cpp']
def read(path):
    try:return path.read_text(encoding='utf-8-sig')
    except UnicodeDecodeError:return path.read_text(encoding='cp1252')
for source in inputs:
    text=read(source).replace('\r\n','\n')
    if source.suffix in ('.cpp','.h'):
        text=re.sub(r'(#\s*include\s*[<"])([^>"\n]+)([>"])',lambda m:m[1]+m[2].lower()+m[3],text)
    (STAGE/source.name.lower()).write_text(text,encoding='utf-8')
(STAGE/'afxres.h').write_text('#include <windows.h>\n#include <winresrc.h>\n#ifndef IDC_STATIC\n#define IDC_STATIC (-1)\n#endif\n',encoding='utf-8')
# The authentic accelerator table is the only resource taken from gens.rc.
# No original icons, images, dialogs, Kaillera resources or core code are linked.
resourceSource=ROOT/'src/win32/src/gens.rc'
match=re.search(r'(?ms)^RAC\s+ACCELERATORS\s*\nBEGIN\n.*?^END\b',read(resourceSource))
if not match:raise SystemExit('Production accelerator table not found')
resourceText='#include "resource.h"\n1 24 "ko_manifest.xml"\n'+match[0]+'\n'
(STAGE/'probe.rc').write_text(resourceText,encoding='utf-8')

def run(command,log):
    result=subprocess.run([str(x) for x in command],cwd=STAGE,env=environment,capture_output=True,text=True,errors='replace')
    (OUT/log).write_text(result.stdout+result.stderr)
    if result.returncode:
        print(result.stdout+result.stderr);result.check_returncode()
    return result
flags=['-std=gnu++17','-O2','-mstackrealign','-fno-strict-aliasing','-DWIN32','-D_WIN32_WINNT=0x0601','-DWINVER=0x0601','-D_MBCS','-D_CRT_SECURE_NO_WARNINGS','-Wno-write-strings','-Wno-narrowing','-I'+str(STAGE)]
objects=[]
for name in ['modern_input','hotkeys','g_dinput','windows_settings_probe']:
    obj=OUT/(name+'.o');objects.append(obj)
    run([compiler,*flags,'-MMD','-MF',OUT/(name+'.d'),'-c',STAGE/(name+'.cpp'),'-o',obj],name+'.log')
resource=OUT/'probe_resource.o';objects.append(resource)
run([resourceCompiler,'--codepage=65001','--preprocessor='+ccompiler,'--preprocessor-arg=-E','--preprocessor-arg=-xc','--preprocessor-arg=-DRC_INVOKED','-I'+str(STAGE),STAGE/'probe.rc','-O','coff','-o',resource],'resource.log')
exe=OUT/'Gens_KO_Settings_Probe.exe'
link=[compiler,'-municode','-mwindows','-static','-static-libgcc','-static-libstdc++','-s','-Wl,--nxcompat,--dynamicbase,--no-insert-timestamp','-Wl,-Map,'+str(OUT/'probe.map'),*objects,
      '-lcomctl32','-ldinput','-ldxguid','-luser32','-lgdi32','-lkernel32','-o',exe]
run(link,'link.log')
inspection=run([objdump,'-p',exe],'pe-inspection.txt')
imports=re.findall(r'DLL Name:\s*(\S+)',inspection.stdout)
allInputs=inputs+[resourceSource,ROOT/'src/common/src/LICENSE.txt',ROOT/'build_settings_probe.py']
hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in allInputs}
report={
    'target':'Windows x86 production-settings component probe',
    'scope':'Real production keyboard/gamepad and transport dialog code, real DirectInput/XInput API calls; no emulator/game execution.',
    'source_inputs':list(hashes),'source_sha256':hashes,
    'production_objects':['modern_input.cpp','hotkeys.cpp','G_dinput.cpp'],
    'harness_objects':['tests/windows_settings_probe.cpp'],
    'stubs':['Controller_*_Type/buttons exported storage','Make_IO_Table no-op: game bus is absent'],
    'resource_scope':'Production ko_manifest.xml and RAC accelerator table only; no icons/bitmaps/core assets',
    'excluded':['Starscream main68k/sub68k cores','32X/Genesis/SegaCD cores','Kaillera','Microsoft Wave sample','mpglib','ROM/BIOS','full emulator binary'],
    'license_scope':'Gens GPL v2 input/frontend source plus Gens KO GPL v2 additions; original LICENSE.txt must accompany source and binary. MinGW runtime notices remain applicable.',
    'compiler':run([compiler,'--version'],'compiler.txt').stdout.splitlines()[0],
    'compile_flags':flags,'imports':imports,
    'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'exe_size_bytes':exe.stat().st_size,
    'windows_execution_verified':False,
}
(OUT/'build-record.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
shutil.copy2(ROOT/'src/common/src/LICENSE.txt',OUT/'LICENSE.txt')
print('BUILT',exe,exe.stat().st_size)
print('SHA256',report['exe_sha256'])
print('DEPENDENCIES',OUT/'build-record.json')
