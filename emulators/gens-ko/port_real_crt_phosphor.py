#!/usr/bin/env python3
"""Deterministic, version-pinned Real CRT Phosphor 1.4.3 -> Gens KO GLSL port.

The converter is new GPL-2.0 code. Generated shader derivatives retain gunkun's
independent custom non-commercial terms in LICENSE.txt and LICENSE_KR.txt.
This converts this exact shader package; it is not a general Slang compiler.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,zipfile

ROOT=Path(__file__).resolve().parent
ARCHIVE_SHA256='d74b48777e733d8c969935bceabd099c0cbfce2dfc149d138fbfc5806db433f1'
UPSTREAM_URL='https://github.com/crt2oldgamer-stack/real-crt-phosphor/releases/tag/v1.4.3'
ARCHIVE_URL='https://github.com/crt2oldgamer-stack/real-crt-phosphor/releases/download/v1.4.3/real-crt-phosphor-v1.4.3.zip'
PINNED_FILES = {'LICENSE.txt': '39ae92b003fdccd391dfd73050be9a2b7357b5fca7703ca3b3fb2b8e5429bb9f',
 'LICENSE_KR.txt': '32664ad7a2167389a3d0a9c2081d3f2cd6798dca3f9dd593d36561567c42de33',
 'README_EN.txt': '08c59a0abbedf7303b7a8d0dca7c52b4ed0c61a23f6ee500a70242475ecda098',
 'README_KR.txt': 'b6933d008719e32c9a9ab583ebc2165f54406daf56503b492747c1feea31b984',
 'presets/4K/4k-base.slangp': '58a091148fffbb06d45ccb3353a41d78202a0e329d0e93e27849c27b87afc64e',
 'presets/4K/4k-ps1-slot.slangp': '667533c8f76a219705d2eacacd8260695d6e7869379f724ec3ed8929602386a3',
 'presets/4K/4k-ps1.slangp': '648bc1afa5f18e550f351d4acfa48e3e9e96ecaf4fea42704bf42798b33de22c',
 'presets/4K/4k-ps2-2d.slangp': 'a8ce98a1f097b09d2cccc009ddbca0c2234c483d9a2b9684ead599252180f5dd',
 'presets/4K/4k-ps2-slot.slangp': 'e36dfd5e5b1ef04031b211558e54db19dda84df9d97f7662c50e959d0f1f72c9',
 'presets/4K/4k-ps2.slangp': 'fbc1a150b74740ba0c20997ee719b3745c10f1bb56176f97c370051a710265d6',
 'presets/4K/4k-psp-slot.slangp': '79028a0898062d52077f968fc9c0751ad614d7f4f8aa584159f940446399d1aa',
 'presets/4K/4k-psp.slangp': 'd518714b60dddb8b5a7c13e0d1eb7a906b4e0db2f0b60ee82ae319a79a42e211',
 'presets/4K/4k-sfc-slot.slangp': '3e2eca2d20b4b1b3f05f67b75a38250b86f2302aee2520e760df4ed609ca5c39',
 'presets/4K/4k-sfc.slangp': 'db855675349ca5be6bf008c1da8eb2d7dc576b9c85e72527e238832e835baf36',
 'presets/4K/4k-up2x.slangp': '3f3feb72fc2927aa8316bc3e5492c3b0dc53d7fbc37fb67dbdae020783f34d3d',
 'presets/4K/4k-up3x.slangp': 'a970d3807484bf5121bb14a6278036fa37499d27d415421d5542d6ccf28b5ac7',
 'presets/4K/4k-up4x.slangp': '1808e61a19ccab5717e48b9d9442c2d78b66ca61e19559e3c558ef7bc4216e1d',
 'presets/Ally/ally-base.slangp': 'c5bb84a1207bff77be95a57d70f94497bbb06162a8fda688e590d3a0bae5bdf9',
 'presets/Ally/ally-ps1-slot.slangp': '45abb3292f3535b58776757c61fc3d479979db444bc01bff6dba422052b5f3e8',
 'presets/Ally/ally-ps1.slangp': 'cb24aa3c3853dead7d198678e026115f028a0fadddb9e70c9d283222fcd5e070',
 'presets/Ally/ally-ps2-2d.slangp': '7b18b3dfa54a514b05d73d16a9c955fd1b868fd325f52333348d7780a60af04f',
 'presets/Ally/ally-ps2-slot.slangp': 'b2232112e59a8690452ecac8d3aa2dc303df6f225930752e8f49483feec978ff',
 'presets/Ally/ally-ps2.slangp': 'e6d29de53b345adcdcf9517973d08d80e2707a33d63059ae13fb471f3dd8f107',
 'presets/Ally/ally-psp-slot.slangp': 'e1fbd2d7d3591890a81ec53b7e6c2b5cd6f8ff243adf65e5e3de2464b40aac48',
 'presets/Ally/ally-psp.slangp': '7458b94699f81d6519832c6e4bf7f18c8f7696e46fbd5327dc4545780a44fbe6',
 'presets/Ally/ally-sfc-slot.slangp': '2fde216b478b4a184af434c12fc815fa4de006ba0a79ec50a7b3c0366a911c3f',
 'presets/Ally/ally-sfc.slangp': 'b05193d847744d5265f3106ab578c594d680677b46e4413fafddfff54524d270',
 'presets/Ally/ally-up2x.slangp': '661ba63d9478f6fa780a289e4a1a01f5c34c54c3574f40fd4758fbe8df9b2a6f',
 'presets/Ally/ally-up3x.slangp': '546c8e092f6e5a12cd64e9f49b792dbbe14e380f41bc0a528fcc9dcd15dbb6ad',
 'presets/Ally/ally-up4x.slangp': '97ab0824350b7cc22dee7680781438797e487dcf25b630d5bb634a25c3a60105',
 'presets/GBA/README_EN.txt': '67896f46fcfc615a66e67e151e98a0fb0355b93bc710ceb4696eb1cce6ff4d30',
 'presets/GBA/README_KR.txt': '4b818c1d40340bf00e807b7f80d9a04f007fd2c34a1dbb7577f0bb971db4bc3e',
 'presets/GBA/gba-crt-light.slangp': '9cf8981ec1deae62b4f4c0c91e820bbd3e16c7cbe4a9a863f76389fffffd9679',
 'presets/GBA/gba-crt-medium.slangp': '9fa06c5d07d5fbadb9ea797c0d02a7b8d29a8c1a30479df0207a29bb96575857',
 'presets/GBA/gba-crt-strong.slangp': 'f3e3b30f9fba5c7e10a41a97042fb681d93f934be1831ac0d939fd5bea8d7238',
 'presets/GBA/gba-light.slangp': 'e7857a57ee8bea0ad94521a39d52e967d0a4c366cc2d8c031331b1dbe63602a5',
 'presets/GBA/gba-medium.slangp': 'cce7acf12c252e77e757d1db70fa0c71b25fd5573c8b4feab97e37ed9d8facb1',
 'presets/GBA/gba-strong.slangp': 'ea5032236a4e2511d02800b6d2e567b3079e4648733e7320f911b1a5c81853d5',
 'real-crt-phosphor-up2x.slangp': '0ade531de41e23c353d0526cf8b959e8062917ef0973a87b7c83e3ca83fa4d22',
 'real-crt-phosphor-up3x.slangp': '96d5f675c25b5482aec9896e5368764ac800a4342c1d541adebe577913d44c10',
 'real-crt-phosphor-up4x.slangp': '0a111a8a3f79ed874c4fca94741a7d3d16f85f5fe4bdd7957e099a8f61f65e01',
 'real-crt-phosphor.slangp': '9e898718c30520144589e94a915e019c7e36dd3b4e7c0300478fbe299adf2842',
 'shaders/ag_mem.slang': 'a352150cfeda8449f6e3d35a1f951fb9b63cc0e17eb9659078da68e7c839ad63',
 'shaders/ag_out.slang': '9f002befe3de9d798943bcd340952e896eebe0e30544faefcd52e0039a32c573',
 'shaders/beam.slang': '336771534c5f34d28c09072adcbe89156f7c5c5e87aff7c6c2c8e5d4de9aedcc',
 'shaders/box.slang': 'a272598bd4fd04c64edc54674b0421023fe86d815dd7942f8d5f81b3b1bc4e0a',
 'shaders/common.inc': '53ad6b073467220c60286f6dadc422abe5d4132c10cdee41f8f413eba2757385',
 'shaders/dither.slang': '7feb0ad09de2ad948ec94e916636808accf14c55962d39ae6dc3bc61b06406f8',
 'shaders/gba-comfort-color.slang': '21fa159d2017a3558333f7a0588e0d09ad0dcf1cb514910204636fe6028bfe84',
 'shaders/glow_h.slang': '431e62269a2b96718b7a748b8426fd256cc3be029b7f0d91d5837b40e3c4fbb6',
 'shaders/glow_v.slang': '3f48673f3490056b796ec7bc19cf119db355ab1b3b7e4874d0f60be28d1799f0',
 'shaders/kern.slang': 'a37f5eadace7d16688d9363783f32ff2a405b3c80fc2e87a827e249035473590',
 'shaders/params.inc': '39584e307077d19b2c6d82098ee663b693e05dac2c79ff55ad81dca18afd5ba0',
 'shaders/post_h.slang': '5436cbb36e619af0881bec55ff2402fe96030b5568d8981ae217bd200d1a79c7',
 'shaders/post_v.slang': '4daf8751746522b82beff0e1980e989338dad2dd591e72ebeaee6cb0c1e47376',
 'shaders/rgb.slang': 'ab872445680af697f8d5732b94333b1dd2ef413a8abd7dbe9c6dd4d2f2f556c0',
 'shaders/signal.slang': 'dd4d16438734052823ab1f0b340deb410a69f99c05c102a39ad18f83dfebe2c5',
 'shaders/soft.slang': '00f0487656c100e015a7c68adf5f2ad723fbf33b2ff23324d72c7b0e75b32445',
 'shaders/ubo.inc': '48a041706f8cee1f40195a8731d4e82c91766386302b4a50b12228d39401dac8',
 'shaders/vblur.slang': '583e6c02499e6db4b7507ae4176f8c6a14865e00fc1c63c42fb293b42a580b0a'}
PARAMETER=re.compile(r'^\s*#pragma\s+parameter\s+(\w+)\s+"[^"]*"\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)',re.M)
ASSIGNMENT=re.compile(r'^([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.*?)\s*$')
INDEXED=re.compile(r'^(shader|alias|filter_linear|wrap_mode|float_framebuffer|srgb_framebuffer|mipmap_input|scale_type|scale_type_x|scale_type_y|scale|scale_x|scale_y|frame_count_mod)(\d+)$')


def sha(data): return hashlib.sha256(data).hexdigest()
def confined(path,root):
    path=path.resolve();path.relative_to(root.resolve());return path

def verify_sources(source,archive=None):
    actual={p.relative_to(source).as_posix():sha(p.read_bytes()) for p in source.rglob('*') if p.is_file()}
    if actual!=PINNED_FILES:
        wrong=sorted(k for k in actual.keys()|PINNED_FILES.keys() if actual.get(k)!=PINNED_FILES.get(k))
        raise ValueError('Source package is not the pinned v1.4.3 archive: '+', '.join(wrong))
    if archive is not None:
        if sha(archive.read_bytes())!=ARCHIVE_SHA256:raise ValueError('Archive SHA-256 mismatch')
        with zipfile.ZipFile(archive) as z:
            entries={n[len('real-crt-phosphor/'):]:sha(z.read(n)) for n in z.namelist() if not n.endswith('/')}
        if entries!=PINNED_FILES:raise ValueError('Archive members differ from the pinned source tree')
    # Filesystem/ZIP extraction order must not affect SOURCE.json bytes.
    return dict(sorted(actual.items()))

def declarations(body):
    fields=[]
    for item in body.split(';'):
        item=item.strip()
        if not item:continue
        m=re.fullmatch(r'(mat4|vec4|float|uint)\s+([\w\s,]+)',item)
        if not m:raise ValueError('Unsupported interface declaration: '+item)
        fields.extend((m[1],n.strip()) for n in m[2].split(','))
    return fields

def convert_shader(text,is_shader):
    """Change interfaces and stage syntax; do not change shader equations."""
    converted=text.replace('\r\n','\n')
    mapping={'SourceSize':'SourceSize','OriginalSize':'KoOriginalSize','OutputSize':'KoOutputSize',
             'FrameCount':'FrameCount','DitherSize':'DitherSize','KernSize':'KernSize','SignalSize':'SignalSize'}
    def push(match):
        result=[]
        for kind,name in declarations(match[1]):
            if name not in mapping:raise ValueError('Unknown push field '+name)
            result.append('uniform '+('int' if kind=='uint' else kind)+' '+mapping[name]+';')
        return '\n'.join(result)
    converted=re.sub(r'layout\s*\(push_constant\)\s*uniform\s+Push\s*\{(.*?)\}\s*params\s*;',push,converted,flags=re.S)
    def uniforms(match):
        return '\n'.join('uniform '+kind+' '+('MVPMatrix' if name=='MVP' else name)+';' for kind,name in declarations(match[1]))
    converted=re.sub(r'layout\s*\(std140,\s*set\s*=\s*0,\s*binding\s*=\s*0\)\s*uniform\s+UBO\s*\{(.*?)\}\s*global\s*;',uniforms,converted,flags=re.S)
    def parameter_ref(match):
        name=match[1]
        if name not in mapping:raise ValueError('Unknown params reference '+name)
        return 'uint(FrameCount)' if name=='FrameCount' else mapping[name]
    converted=re.sub(r'(?<!["\w])params\.(\w+)',parameter_ref,converted)
    converted=re.sub(r'\bglobal\.(\w+)',lambda m:'MVPMatrix' if m[1]=='MVP' else m[1],converted)
    samplers=re.findall(r'\buniform\s+sampler2D\s+(\w+)\s*;',converted)
    for name in sorted(set(samplers),key=len,reverse=True):
        replacement='Source' if name=='Source' else 'FeedbackTexture' if name=='AGMemFeedback' else name+'Texture'
        converted=re.sub(r'\b'+re.escape(name)+r'\b',replacement,converted)
    converted=re.sub(r'layout\s*\([^)]*\)\s*','',converted)
    converted=re.sub(r'\bPosition\b','VertexCoord',converted)
    if is_shader:
        if len(re.findall(r'^\s*#pragma stage vertex\s*$',converted,re.M))!=1 or len(re.findall(r'^\s*#pragma stage fragment\s*$',converted,re.M))!=1:
            raise ValueError('Unexpected shader stage structure')
        converted=re.sub(r'^[ \t]*#version[ \t]+450[ \t]*$', '#version 130',converted,flags=re.M)
        converted=re.sub(r'^[ \t]*#pragma stage vertex[ \t]*$', '#if defined(VERTEX)',converted,flags=re.M)
        converted=re.sub(r'^[ \t]*#pragma stage fragment[ \t]*$', '#elif defined(FRAGMENT)',converted,flags=re.M)
        converted+='\n#endif\n'
        converted=converted.replace('#version 130\n','#version 130\n// Gens KO dedicated GLSL port, 2026-11-20. Original equations and pass order retained.\n// This shader retains gunkun\'s custom terms; see ../LICENSE.txt and ../LICENSE_KR.txt.\n',1)
    if re.search(r'(?<!["\w])(params|global)\.',converted) or '#pragma stage' in converted or 'layout(' in converted:
        raise ValueError('Unconverted Slang interface remains')
    return converted

def strip_comment(line):
    quoted=False
    for i,ch in enumerate(line):
        if ch=='"' and (i==0 or line[i-1]!='\\'):quoted=not quoted
        elif ch=='#' and not quoted:return line[:i]
    return line

def parse_preset(path):
    own={};references=[]
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        line=line.strip()
        if line.startswith('#reference'):
            m=re.fullmatch(r'#reference\s+"([^"]+)"\s*',line)
            if not m:raise ValueError('Bad preset reference in '+str(path))
            references.append(m[1]);continue
        line=strip_comment(line).strip()
        if not line:continue
        m=ASSIGNMENT.fullmatch(line)
        if not m:raise ValueError('Bad preset assignment '+line)
        key,value=m.groups()
        if value.startswith('"'):
            if not value.endswith('"'):raise ValueError('Unclosed preset string')
            value=value[1:-1]
        if key in own:raise ValueError('Duplicate preset key '+key)
        own[key]=value
    return references,own

def flatten_preset(path,source,stack=()):
    path=confined(path,source)
    if path in stack or len(stack)>12:raise ValueError('Preset inheritance cycle/depth')
    references,own=parse_preset(path);values={};origins={};chain=[]
    for name in references:
        base=confined(path.parent/name,source)
        inherited,located,ancestors=flatten_preset(base,source,stack+(path,))
        values.update(inherited);origins.update(located);chain.extend(ancestors)
    values.update(own)
    for key,value in own.items():
        if re.fullmatch(r'shader\d+',key):origins[key]=confined(path.parent/value,source)
    chain.append(path.relative_to(source).as_posix())
    return values,origins,chain

def expand_includes(path,root,stack=()):
    path=confined(path,root)
    if path in stack or len(stack)>8:raise ValueError('Shader include cycle/depth')
    def include(match):
        return expand_includes(path.parent/match[1],root,stack+(path,))
    return re.sub(r'^\s*#include\s+"([^"]+)"\s*$',include,path.read_text(encoding='utf-8-sig'),flags=re.M)

def convert_preset(path,source,output):
    values,origins,chain=flatten_preset(path,source)
    count=int(values['shaders'])
    if count not in (1,14,15):raise ValueError('Unexpected v1.4.3 pass count')
    result={'shaders':str(count)};params={};feedback=[]
    destination=output/path.relative_to(source).with_suffix('.glslp')
    for i in range(count):
        key='shader'+str(i)
        original=origins[key]
        for match in PARAMETER.finditer(expand_includes(original,source)):
            name,default,minimum,maximum=match.groups()
            if name in params and params[name]!=(default,minimum,maximum):raise ValueError('Inconsistent parameter '+name)
            params[name]=(default,minimum,maximum)
        target=output/original.relative_to(source).with_suffix('.glsl')
        result[key]=Path(os.path.relpath(target,destination.parent)).as_posix()
        if original.name=='ag_mem.slang':feedback.append(i)
        for name,value in values.items():
            m=INDEXED.fullmatch(name)
            if m and int(m[2])==i and m[1]!='shader':result[name]=value
    if len(feedback)>1:raise ValueError('Ambiguous feedback pass')
    if feedback:result['feedback_pass']=str(feedback[0])
    unsupported=[k for k in values if k not in ('shaders','parameters') and not INDEXED.fullmatch(k) and k not in params]
    if unsupported:raise ValueError('Unknown upstream preset keys: '+', '.join(unsupported))
    # Include every exposed parameter. In particular, inherited numeric overrides
    # must not disappear because the original .slangp omitted a parameters list.
    names=sorted(params);result['parameters']=';'.join(names)
    for name in names:
        value=values.get(name,params[name][0]);float(value)
        result[name]=value
    lines=['# Real CRT Phosphor v1.4.3 by gunkun (건군).',
           '# Dedicated Gens KO GLSL port, 2026-11-20; original custom terms apply.',
           '# Original preset: '+path.relative_to(source).as_posix(),
           '# Inheritance flattened: '+' -> '.join(chain)]
    lines.extend(key+' = '+json.dumps(value,ensure_ascii=False) for key,value in result.items())
    record={'original':path.relative_to(source).as_posix(),'converted':destination.relative_to(output).as_posix(),
            'inheritance':chain,'passes':count,'feedback_pass':feedback[0] if feedback else None,
            'parameters':len(names),'override_names':sorted(k for k in values if k in params),
            'shader_sources':[origins['shader'+str(i)].relative_to(source).as_posix() for i in range(count)]}
    return '\n'.join(lines)+'\n',record

PORT_NOTES='''Real CRT Phosphor v1.4.3 — Gens KO 전용 GLSL 포트

원작자: 건군(gunkun). 기획·화면 방향·실기 테스트·프리셋 조정은 원작자의 작업입니다.
원본: https://github.com/crt2oldgamer-stack/real-crt-phosphor/releases/tag/v1.4.3
원본 코드의 AI 저작물 표시는 원본 LICENSE.txt / LICENSE_KR.txt에 그대로 보존했습니다.

이 폴더는 원본 Slang 셰이더를 Gens KO의 OpenGL 인터페이스에 맞춘 전용 포트입니다.
범용 Slang 로더가 아니며 .slangp 파일을 직접 실행하는 기능도 아닙니다.
원본 14패스, GBA CRT 15패스, 단일 패스 GBA 색 보정과 원본 수식·상수를 보존합니다.
GLSL 1.30으로 단계/입력 선언을 바꾸고, float 중간 버퍼와 잔광 피드백을 사용합니다.
모든 #reference 상속은 상대 경로와 설정 우선순위를 유지하여 .glslp에 평탄화했습니다.

사용: Gens KO의 화면 출력 및 셰이더 설정에서 이 폴더의
real-crt-phosphor.glslp 또는 presets/4K, presets/Ally 아래의 .glslp를 불러옵니다.
기본 프리셋의 AFTERGLOW(잔상)와 CURVATURE(곡률)는 원본처럼 0입니다.
프리셋을 적용한 뒤 '세부 설정'에서 숫자를 바꾸고 '적용·저장'을 누르세요.
설정값은 Gens-KO-shaders.ini에 프리셋별로 보관합니다.
OpenGL 3.0/GLSL 1.30 이상 및 실수형 프레임버퍼 지원이 필요합니다.
GDI 호환 출력에서는 셰이더를 적용할 수 없습니다.

이 셰이더는 Gens KO 프로그램의 GPL 라이선스로 재허가되지 않습니다.
원작자의 별도 비상업적 이용 조건을 그대로 따릅니다. 원문 LICENSE 두 파일을 확인하세요.
재배포 시 원작자와 Real CRT Phosphor 이름, 원문 이용 조건, 이 포팅 고지를 보존하세요.
수정일: 2026-11-20. 변환 소스: port_real_crt_phosphor.py.
SOURCE.json에는 원본 ZIP 및 각 원본/포트 파일의 SHA-256을 기록했습니다.
원본 README_EN.txt / README_KR.txt는 원작자의 Slang 설치 안내이므로,
Gens KO에서는 위 .glslp 사용 방법을 따르세요.
'''

def make_outputs(source,output,archive=None):
    original_hashes=verify_sources(source,archive);files={};presets=[]
    for path in sorted(p for p in source.rglob('*') if p.is_file()):
        rel=path.relative_to(source)
        if path.suffix=='.slang':files[rel.with_suffix('.glsl').as_posix()]=convert_shader(path.read_text(encoding='utf-8-sig'),True).encode()
        elif path.suffix=='.inc':files[rel.as_posix()]=convert_shader(path.read_text(encoding='utf-8-sig'),False).encode()
        elif path.suffix=='.slangp':
            text,record=convert_preset(path,source,output);files[rel.with_suffix('.glslp').as_posix()]=text.encode();presets.append(record)
        else:files[rel.as_posix()]=path.read_bytes()
    files['README_PORT_KO.txt']=PORT_NOTES.encode()
    provenance={'name':'Real CRT Phosphor','upstream_version':'1.4.3','author':'gunkun (건군)',
                'upstream_release':UPSTREAM_URL,'upstream_archive':ARCHIVE_URL,'upstream_archive_sha256':ARCHIVE_SHA256,
                'upstream_asset_id':747786504,'upstream_published_utc':'2026-11-14T14:39:41Z',
                'port_date':'2026-11-20','port_target':'Gens KO dedicated GLSL 1.30 port; not general Slang support',
                'terms':'Original custom non-commercial terms retained; not relicensed under Gens GPL',
                'converter_sha256':sha(Path(__file__).read_bytes()),'source_files_sha256':original_hashes,
                'converted_files_sha256':{name:sha(data) for name,data in sorted(files.items())},'presets':presets}
    files['SOURCE.json']=(json.dumps(provenance,ensure_ascii=False,indent=2)+'\n').encode()
    return files,provenance

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=ROOT/'upstream/real-crt-phosphor-v1.4.3/real-crt-phosphor')
    parser.add_argument('--output',type=Path,default=ROOT/'src/shaders/real-crt-phosphor')
    parser.add_argument('--archive',type=Path)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args();source=args.source.resolve();output=args.output.resolve()
    archive=args.archive
    if archive is None:
        candidate=ROOT.parent/'upload/real-crt-phosphor-v1.4.3.zip'
        if candidate.exists():archive=candidate
    files,record=make_outputs(source,output,archive)
    if args.check:
        actual={p.relative_to(output).as_posix():p.read_bytes() for p in output.rglob('*') if p.is_file()}
        if actual!=files:raise SystemExit('Generated port differs from the deterministic converter output')
    else:
        for name,data in files.items():
            path=output/name;path.parent.mkdir(parents=True,exist_ok=True)
            if not path.exists() or path.read_bytes()!=data:path.write_bytes(data)
    print(json.dumps({'status':'verified' if args.check else 'generated','output':str(output),
                      'files':len(files),'glsl_files':sum(x.endswith('.glsl') for x in files),
                      'presets':len(record['presets']),'archive_sha256':ARCHIVE_SHA256},ensure_ascii=False))
if __name__=='__main__':main()
