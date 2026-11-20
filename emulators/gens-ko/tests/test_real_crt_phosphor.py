#!/usr/bin/env python3
"""Verify the dedicated GLSL port against original shader equations via EGL.

The reference path compiles original GLSL450 bodies with their original UBO and
push block fields; only Vulkan layout/storage syntax and stage splitting are
adapted to desktop GL. The port path is GLSL130/plain uniforms. This is not a
RetroArch/Win32 execution test and does not claim their exact driver output.
"""
from pathlib import Path
import hashlib,json,re,sys,time,subprocess
import numpy as np
from PIL import Image
import real_crt_egl as gl
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'upstream/real-crt-phosphor-v1.4.3/real-crt-phosphor'
PORT=ROOT/'src/shaders/real-crt-phosphor'
OUT=ROOT/'build/real_crt_phosphor';OUT.mkdir(parents=True,exist_ok=True)
report={'scope':'Native surfaceless EGL: original GLSL450 equations/UBO interfaces vs dedicated GLSL130 port; same RGBA8/RGBA16F texture policy.',
        'gl_vendor':gl.GetString(0x1F00).decode(),'gl_renderer':gl.GetString(0x1F01).decode(),'gl_version':gl.GetString(0x1F02).decode(),
        'limitations':['Not a RetroArch runtime capture.','Not the Windows/WGL Gens renderer.','Performance on physical GPUs and all 4K outputs is not measured.'],
        'structural':{},'cases':[]}
start=time.time()

def check(condition,message):
    if not condition:raise AssertionError(message)
def pattern(w,h):
    y,x=np.indices((h,w));im=np.zeros((h,w,4),np.uint8);im[:,:,3]=255
    palette=np.array([[255,0,0],[0,255,0],[0,0,255],[255,255,255],[0,255,255],[255,0,255],[255,255,0],[128,128,128]],np.uint8)
    top=y<h//3;im[top,:3]=palette[np.minimum(7,x[top]*8//w)]
    mid=(y>=h//3)&(y<2*h//3);im[mid,0]=(x[mid]*255//max(w-1,1));im[mid,1]=(y[mid]*255//max(h-1,1));im[mid,2]=180
    bottom=y>=2*h//3;ch=((x//4+y//4)&1)!=0;im[bottom&ch,:3]=(235,195,55);im[bottom&~ch,:3]=(10,24,50)
    im[0,:,:3]=im[-1,:,:3]=255;im[:,0,:3]=im[:,-1,:3]=255
    return im

def structural():
    subprocess.run([sys.executable,str(ROOT/'port_real_crt_phosphor.py'),'--check'],check=True)
    sources=sorted(SOURCE.rglob('*.slangp'));check(len(sources)==36,'All upstream presets must be covered')
    total=0
    for source in sources:
        target=PORT/source.relative_to(SOURCE).with_suffix('.glslp')
        original=gl.read_preset(source);ported=gl.read_preset(target)
        check(original['shaders']==ported['shaders'],'Pass count changed: '+str(source))
        count=int(original['shaders']);parameter_names=set(ported['parameters'].split(';'));defaults={}
        for index in range(count):
            old=Path(original['shader'+str(index)]);new=Path(ported['shader'+str(index)])
            check(old.relative_to(SOURCE).with_suffix('.glsl')==new.relative_to(PORT),'Inherited shader path was rebased incorrectly')
            check(new.is_file(),'Missing converted shader')
            defaults.update(gl.defaults(old))
        for key,value in original.items():
            if re.fullmatch(r'shader\d+',key) or key=='parameters':continue
            if key in defaults:
                check(key in parameter_names and float(ported[key])==float(value),'Lost parameter override: '+key)
            else:check(ported[key]==value,'Preset setting changed: '+key)
        for key,(default,minimum,maximum) in defaults.items():
            check(key in parameter_names and float(ported[key])==float(original.get(key,default)),'Parameter mismatch: '+key)
        feedback=[i for i in range(count) if Path(original['shader'+str(i)]).stem=='ag_mem']
        check(int(ported.get('feedback_pass','-1'))==(feedback[0] if feedback else -1),'Wrong feedback index')
        total+=count
    for name in ['LICENSE.txt','LICENSE_KR.txt']:
        check((SOURCE/name).read_bytes()==(PORT/name).read_bytes(),'Original terms changed')
    shader_sources=sorted((SOURCE/'shaders').glob('*.slang'))
    for source in shader_sources:
        target=PORT/source.relative_to(SOURCE).with_suffix('.glsl')
        gl.program_for(source,True);gl.program_for(target,False)
    report['structural']={'presets':len(sources),'total_preset_pass_entries':total,'original_and_port_shader_pairs_compiled':len(shader_sources),
        'inheritance_paths_all_overrides_feedback_and_original_licenses':'pass'}
    print('STRUCTURAL PASS',report['structural'],flush=True)

def compare(label,relative,image,viewport=(960,720),overrides=None,save=True):
    original=gl.Pipeline(SOURCE/relative,True,viewport,overrides)
    converted=gl.Pipeline(PORT/Path(relative).with_suffix('.glslp'),False,viewport,overrides)
    try:
        old=original.render(image,1);new=converted.render(image,1)
        diff=np.abs(old.astype(np.int16)-new.astype(np.int16));maxdiff=int(diff.max());changed=int(np.count_nonzero(np.any(diff[:,:,:3],axis=2)))
        check(maxdiff<=1,label+' diverged by more than one RGBA8 code value: '+str(maxdiff))
        check(np.any(new[:,:,:3]),label+' produced a black output')
        item={'case':label,'preset':relative,'input_size':[image.shape[1],image.shape[0]],'output_size':[new.shape[1],new.shape[0]],
              'passes':original.count,'max_rgba8_abs_difference':maxdiff,'mean_rgb_abs_difference':float(diff[:,:,:3].mean()),
              'changed_rgb_pixels':changed,'reference_sha256':hashlib.sha256(old.tobytes()).hexdigest(),'port_sha256':hashlib.sha256(new.tobytes()).hexdigest(),'status':'pass'}
        report['cases'].append(item)
        if save:
            Image.fromarray(old,'RGBA').save(OUT/(label+'-reference.png'));Image.fromarray(new,'RGBA').save(OUT/(label+'-port.png'))
        print(label,'PASS maxdelta',maxdiff,'changed',changed,flush=True)
        return new
    finally:original.close();converted.close()

def afterglow():
    viewport=(640,480);opts={'AFTERGLOW':0.8,'AG_TIME':200,'AG_RED':2.0,'MASK_PITCH':4.5}
    a=gl.Pipeline(SOURCE/'real-crt-phosphor.slangp',True,viewport,opts)
    b=gl.Pipeline(PORT/'real-crt-phosphor.glslp',False,viewport,opts)
    dark=np.zeros((240,320,4),np.uint8);dark[:,:,3]=255;flash=dark.copy();flash[70:170,110:210,:3]=255
    levels=[];deltas=[]
    try:
        for frame,image in enumerate([flash,dark,dark,dark,dark],1):
            old=a.render(image,frame);new=b.render(image,frame)
            delta=int(np.abs(old.astype(np.int16)-new.astype(np.int16)).max());check(delta<=1,'Afterglow reference mismatch')
            levels.append(float(new[:,:,:3].mean()));deltas.append(delta)
            if frame in (1,2,5):Image.fromarray(new,'RGBA').save(OUT/('afterglow-frame-'+str(frame)+'.png'))
        check(levels[1]>0.5,'Afterglow did not retain a visible previous frame')
        check(all(x>y for x,y in zip(levels[1:],levels[2:])),'Afterglow did not decay')
        repeat=b.render(dark,5)
        check(np.array_equal(repeat,new),'A same-frame repaint advanced afterglow history')
        old=a.render(np.zeros((224,256,4),np.uint8),6);resized=b.render(np.zeros((224,256,4),np.uint8),6)
        check(not np.any(resized[:,:,:3]),'Resize retained stale feedback')
        check(np.max(np.abs(old.astype(np.int16)-resized.astype(np.int16)))<=1,'Resize reference mismatch')
        report['cases'].append({'case':'afterglow-sequence-repaint-resize','status':'pass','rgb_mean_per_frame':levels,'max_differences':deltas,
            'same_frame_repaint_identical':True,'resize_history_cleared':True,'scope':'Independent EGL pipeline contract; Windows renderer path not executed'})
        print('afterglow sequence / repaint / resize PASS',levels,flush=True)
    finally:a.close();b.close()

def float_guard():
    vertex='#version 130\nin vec4 VertexCoord;uniform mat4 MVPMatrix;void main(){gl_Position=MVPMatrix*VertexCoord;}\n'
    fragment='#version 130\nout vec4 FragColor;void main(){FragColor=vec4(-0.5,1.5,2.25,0.25);}\n'
    program=gl.Program('float-guard',False,(vertex,fragment));target=gl.Texture(4,4,True)
    try:
        gl.BindFbo(0x8D40,target.fbo);gl.Viewport(0,0,4,4)
        program.bind(None,{},None,{'SourceSize':gl.wh((4,4)),'OriginalSize':gl.wh((4,4)),'OutputSize':gl.wh((4,4))},{},1)
        data=target.read(True);check(np.allclose(data,(-0.5,1.5,2.25,0.25),rtol=0,atol=0),'Float FBO clamped negative/HDR values')
        report['cases'].append({'case':'rgba16f-signed-hdr','status':'pass','sample':data[0,0].tolist()})
        print('RGBA16F negative / HDR PASS',flush=True)
    finally:target.close();program.close()

def main():
    structural();float_guard()
    image=pattern(320,240);Image.fromarray(image,'RGBA').save(OUT/'input.png')
    base=compare('base','real-crt-phosphor.slangp',image)
    changed=compare('slot-curvature','real-crt-phosphor.slangp',image,overrides={'MASK_TYPE':1,'CURVATURE':0.2,'MASK_CURVE':1,'MASK_PITCH':4.5})
    check(np.count_nonzero(base[:,:,:3]!=changed[:,:,:3])>1000,'Slot mask / curvature controls do not affect output')
    compare('wide-512','real-crt-phosphor.slangp',pattern(512,240),save=False)
    compare('hires-640','real-crt-phosphor.slangp',pattern(640,480),save=False)
    compare('up2x','real-crt-phosphor-up2x.slangp',np.repeat(np.repeat(image,2,0),2,1),save=False)
    compare('up3x','real-crt-phosphor-up3x.slangp',np.repeat(np.repeat(image,3,0),3,1),save=False)
    compare('up4x','real-crt-phosphor-up4x.slangp',np.repeat(np.repeat(image,4,0),4,1),save=False)
    compare('4k-slot-preset','presets/4K/4k-sfc-slot.slangp',image,save=False)
    compare('ally-psp-preset','presets/Ally/ally-psp.slangp',pattern(480,272),save=False)
    compare('gba-crt-medium','presets/GBA/gba-crt-medium.slangp',pattern(240,160),(720,480))
    compare('gba-color-medium','presets/GBA/gba-medium.slangp',pattern(240,160),(720,480),save=False)
    afterglow()
    report['status']='pass';report['elapsed_seconds']=round(time.time()-start,3)
    files=[ROOT/'port_real_crt_phosphor.py',Path(__file__),ROOT/'tests/real_crt_egl.py']+sorted(p for p in PORT.rglob('*') if p.is_file())
    report['source_sha256']={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    (OUT/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    gl.shutdown();print('ALL PASS',OUT/'report.json',flush=True)
if __name__=='__main__':
    try:main()
    except Exception as exc:
        report['status']='fail';report['error']=repr(exc);report['elapsed_seconds']=round(time.time()-start,3)
        (OUT/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
        raise
