#!/usr/bin/env python3
"""Package only this curated source subset and genuine settings-probe captures."""
from pathlib import Path
import hashlib, json, zipfile, shutil

ROOT=Path(__file__).resolve().parent
OUT=Path.cwd()/'release-assets'
OUT.mkdir(exist_ok=True)
TAG='Gens_KO_2.14_R2_modules'
PROOF=Path.cwd()/'settings-proof'
report=json.loads((PROOF/'windows-smoke.json').read_text(encoding='utf-8-sig'))
if report.get('status')!='PASS' or report.get('emulator_startup_verified') is not False:
    raise SystemExit('A successful, explicitly scoped settings-component capture is required.')

def put_zip(folder,path,prefix,allowed=None):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,9) as archive:
        for file in sorted(folder.rglob('*')):
            if not file.is_file():continue
            rel=file.relative_to(folder)
            if any(part in ('build','.git','__pycache__') for part in rel.parts):continue
            if allowed is not None and str(rel) not in allowed:continue
            info=zipfile.ZipInfo(prefix+'/'+str(rel),(2026,11,20,0,0,0))
            info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
            archive.writestr(info,file.read_bytes())

manifest=json.loads((ROOT/'PUBLIC_SOURCE_MANIFEST.json').read_text())
for rel,digest in manifest['sha256'].items():
    if hashlib.sha256((ROOT/rel).read_bytes()).hexdigest()!=digest:
        raise SystemExit('Source changed after review: '+rel)
allowed=set(manifest['sha256'])|{'PUBLIC_SOURCE_MANIFEST.json'}
put_zip(ROOT,OUT/(TAG+'_source.zip'),'Gens_KO_modules',allowed)
for item in report['screenshots']:
    source=PROOF/item['file']
    if hashlib.sha256(source.read_bytes()).hexdigest()!=item['sha256']:
        raise SystemExit('Screenshot hash mismatch: '+item['file'])
put_zip(PROOF,OUT/(TAG+'_settings_screenshots.zip'),'Gens_KO_settings_proof')
shutil.copyfile(ROOT/'FORUM_POST_KO.txt',OUT/(TAG+'_FORUM_POST_KO.txt'))
shutil.copyfile(ROOT/'PUBLIC_SCOPE_KO.txt',OUT/(TAG+'_PUBLIC_SCOPE_KO.txt'))
checksum=OUT/(TAG+'_SHA256.txt')
checksum.write_text('\n'.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name
    for p in sorted(OUT.iterdir()) if p.is_file() and p!=checksum)+'\n')
print('Packaged source subset and genuine settings captures; no full emulator binary or game ROM.')
