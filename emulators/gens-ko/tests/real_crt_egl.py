#!/usr/bin/env python3
"""Native EGL support for the version-pinned Real CRT Phosphor port tests.

This validates shader compilation, parameters and effects. It does NOT execute
the Win32/WGL window shell and does not prove physical multi-monitor behavior.
"""
from pathlib import Path
import ctypes as C, ctypes.util, os, re, json, hashlib, sys
os.environ['LIBGL_ALWAYS_SOFTWARE']='1'
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'build/real_crt_phosphor';OUT.mkdir(parents=True,exist_ok=True)
E=C.CDLL(ctypes.util.find_library('EGL'))
def ef(name,restype,args):
    f=getattr(E,name);f.restype=restype;f.argtypes=args;return f
getproc=ef('eglGetProcAddress',C.c_void_p,[C.c_char_p])
getdisplay=C.CFUNCTYPE(C.c_void_p,C.c_uint,C.c_void_p,C.POINTER(C.c_int))(getproc(b'eglGetPlatformDisplayEXT'))
display=getdisplay(0x31DD,None,None) # EGL_PLATFORM_SURFACELESS_MESA
major=C.c_int();minor=C.c_int()
assert ef('eglInitialize',C.c_uint,[C.c_void_p,C.POINTER(C.c_int),C.POINTER(C.c_int)])(display,C.byref(major),C.byref(minor))
assert ef('eglBindAPI',C.c_uint,[C.c_uint])(0x30A2) # desktop OpenGL
attrs=(C.c_int*15)(0x3024,8,0x3023,8,0x3022,8,0x3021,8,0x3040,8,0x3033,1,0x3038,0,0)
config=C.c_void_p();count=C.c_int()
assert ef('eglChooseConfig',C.c_uint,[C.c_void_p,C.POINTER(C.c_int),C.POINTER(C.c_void_p),C.c_int,C.POINTER(C.c_int)])(display,attrs,C.byref(config),1,C.byref(count)) and count.value
surfaceattrs=(C.c_int*5)(0x3057,960,0x3056,720,0x3038)
surface=ef('eglCreatePbufferSurface',C.c_void_p,[C.c_void_p,C.c_void_p,C.POINTER(C.c_int)])(display,config,surfaceattrs)
context=ef('eglCreateContext',C.c_void_p,[C.c_void_p,C.c_void_p,C.c_void_p,C.POINTER(C.c_int)])(display,config,None,None)
assert surface and context
assert ef('eglMakeCurrent',C.c_uint,[C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p])(display,surface,surface,context)
G=C.CDLL(ctypes.util.find_library('GL'))
def gf(name,restype,args):
    try:f=getattr(G,name);f.restype=restype;f.argtypes=args;return f
    except AttributeError:return C.CFUNCTYPE(restype,*args)(getproc(name.encode()))
ui=C.c_uint;ii=C.c_int;ff=C.c_float;ptr=C.c_void_p
GetString=gf('glGetString',C.c_char_p,[ui]);GetError=gf('glGetError',ui,[])
CreateShader=gf('glCreateShader',ui,[ui]);ShaderSource=gf('glShaderSource',None,[ui,ii,C.POINTER(C.c_char_p),C.POINTER(ii)])
CompileShader=gf('glCompileShader',None,[ui]);GetShaderiv=gf('glGetShaderiv',None,[ui,ui,C.POINTER(ii)])
GetShaderInfoLog=gf('glGetShaderInfoLog',None,[ui,ii,C.POINTER(ii),ptr]);DeleteShader=gf('glDeleteShader',None,[ui])
CreateProgram=gf('glCreateProgram',ui,[]);AttachShader=gf('glAttachShader',None,[ui,ui]);LinkProgram=gf('glLinkProgram',None,[ui])
GetProgramiv=gf('glGetProgramiv',None,[ui,ui,C.POINTER(ii)]);GetProgramInfoLog=gf('glGetProgramInfoLog',None,[ui,ii,C.POINTER(ii),ptr])
UseProgram=gf('glUseProgram',None,[ui]);DeleteProgram=gf('glDeleteProgram',None,[ui]);GetUniform=gf('glGetUniformLocation',ii,[ui,C.c_char_p])
Uniform1i=gf('glUniform1i',None,[ii,ii]);Uniform1f=gf('glUniform1f',None,[ii,ff]);Uniform2f=gf('glUniform2f',None,[ii,ff,ff]);UniformMatrix=gf('glUniformMatrix4fv',None,[ii,ii,C.c_ubyte,C.POINTER(ff)])
GetAttrib=gf('glGetAttribLocation',ii,[ui,C.c_char_p]);EnableAttrib=gf('glEnableVertexAttribArray',None,[ui]);DisableAttrib=gf('glDisableVertexAttribArray',None,[ui]);AttribPointer=gf('glVertexAttribPointer',None,[ui,ii,ui,C.c_ubyte,ii,ptr])
GenTextures=gf('glGenTextures',None,[ii,C.POINTER(ui)]);BindTexture=gf('glBindTexture',None,[ui,ui]);TexImage=gf('glTexImage2D',None,[ui,ii,ii,ii,ii,ii,ui,ui,ptr]);TexParameteri=gf('glTexParameteri',None,[ui,ui,ii]);DeleteTextures=gf('glDeleteTextures',None,[ii,C.POINTER(ui)])
ActiveTexture=gf('glActiveTexture',None,[ui]);GenFbos=gf('glGenFramebuffers',None,[ii,C.POINTER(ui)]);BindFbo=gf('glBindFramebuffer',None,[ui,ui]);AttachTexture=gf('glFramebufferTexture2D',None,[ui,ui,ui,ui,ii]);CheckFbo=gf('glCheckFramebufferStatus',ui,[ui]);DeleteFbos=gf('glDeleteFramebuffers',None,[ii,C.POINTER(ui)])
Viewport=gf('glViewport',None,[ii,ii,ii,ii]);ClearColor=gf('glClearColor',None,[ff,ff,ff,ff]);Clear=gf('glClear',None,[ui]);DrawArrays=gf('glDrawArrays',None,[ui,ii,ii]);ReadPixels=gf('glReadPixels',None,[ii,ii,ii,ii,ui,ui,ptr]);Finish=gf('glFinish',None,[])

# Extra GL interfaces used to keep the original shader's UBO layout independent
# of the port's plain-uniform binding path.
import struct
import numpy as np
Uniform4f=gf('glUniform4f',None,[ii,ff,ff,ff,ff])
GetActiveUniform=gf('glGetActiveUniform',None,[ui,ui,ii,C.POINTER(ii),C.POINTER(ii),C.POINTER(ui),ptr])
GetActiveUniformsiv=gf('glGetActiveUniformsiv',None,[ui,ii,C.POINTER(ui),ui,C.POINTER(ii)])
GetActiveBlockiv=gf('glGetActiveUniformBlockiv',None,[ui,ui,ui,C.POINTER(ii)])
GetActiveBlockName=gf('glGetActiveUniformBlockName',None,[ui,ui,ii,C.POINTER(ii),ptr])
UniformBlockBinding=gf('glUniformBlockBinding',None,[ui,ui,ui])
GenBuffers=gf('glGenBuffers',None,[ii,C.POINTER(ui)]);BindBuffer=gf('glBindBuffer',None,[ui,ui])
BufferData=gf('glBufferData',None,[ui,C.c_ssize_t,ptr,ui]);BindBufferBase=gf('glBindBufferBase',None,[ui,ui,ui])
DeleteBuffers=gf('glDeleteBuffers',None,[ii,C.POINTER(ui)])
TexSubImage=gf('glTexSubImage2D',None,[ui,ii,ii,ii,ii,ii,ui,ui,ptr])
ClampColor=gf('glClampColor',None,[ui,ui]);Disable=gf('glDisable',None,[ui])
for clamp in [0x891A,0x891B,0x891C]:ClampColor(clamp,0)
for state in [0x0B71,0x0BE2,0x0B44,0x0BD0,0x8DB9]:Disable(state)
MATRIX=(2,0,0,0,0,2,0,0,0,0,1,0,-1,-1,0,1)
VERTEX=(ff*16)(0,0,0,1,1,0,0,1,0,1,0,1,1,1,0,1)
UV=(ff*8)(0,0,1,0,0,1,1,1)

def expanded(path,stack=()):
    path=Path(path).resolve()
    if path in stack or len(stack)>8:raise ValueError('include recursion')
    rows=[]
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        match=re.fullmatch(r'\s*#include\s+"([^"]+)"\s*',line)
        rows.append(expanded(path.parent/match[1],stack+(path,)) if match else line)
    return '\n'.join(rows)+'\n'

def original_stages(path):
    source=expanded(path)
    # Keep original global and params uniform blocks and all formula bodies.
    # The only Vulkan-specific adaptation is descriptor/push storage syntax.
    source=re.sub(r'layout\s*\(push_constant\)', 'layout(std140, binding = 1)',source)
    source=re.sub(r'\bset\s*=\s*0\s*,\s*','',source)
    common,rest=source.split('#pragma stage vertex',1)
    vertex,fragment=rest.split('#pragma stage fragment',1)
    clean=lambda x:re.sub(r'^[ \t]*#pragma parameter[^\n]*','',x,flags=re.M)
    return clean(common+vertex),clean(common+fragment)

def port_stages(path):
    source=expanded(path);version=re.search(r'^#version[^\n]*',source,re.M)[0]
    body=re.sub(r'^#version[^\n]*','',source,flags=re.M)
    body=re.sub(r'^[ \t]*#pragma parameter[^\n]*','',body,flags=re.M)
    return tuple(version+'\n#define '+stage+' 1\n#define PARAMETER_UNIFORM 1\n'+body for stage in ['VERTEX','FRAGMENT'])

def defaults(path):
    result={}
    for m in re.finditer(r'#pragma\s+parameter\s+(\w+)\s+"[^"]*"\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)',expanded(path)):
        result[m[1]]=(float(m[2]),float(m[3]),float(m[4]))
    return result

def read_preset(path,stack=()):
    # Independent line resolver: shader paths are anchored where each override
    # is declared, before inheritance merges. No converter helper is imported.
    path=Path(path).resolve()
    if path in stack:raise ValueError('preset recursion')
    refs=[];assign={}
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        line=line.strip()
        if line.startswith('#reference'):
            refs.append(path.parent/re.fullmatch(r'#reference\s+"([^"]+)"',line)[1]);continue
        if not line or line.startswith('#'):continue
        key,value=line.split('=',1);key=key.strip();value=value.strip().strip('"')
        assign[key]=str((path.parent/value).resolve()) if re.fullmatch(r'shader\d+',key) else value
    values={}
    for ref in refs:values.update(read_preset(ref,stack+(path,)))
    values.update(assign);return values

def wh(size):return (float(size[0]),float(size[1]),1.0/size[0],1.0/size[1])

class Program:
    def __init__(self,path,reference=False,stage_text=None):
        self.path=Path(path);self.reference=reference;self.blocks=[];self.uniforms=[];self.samplers=[]
        stages=stage_text or (original_stages(path) if reference else port_stages(path))
        compiled=[]
        for text,kind in zip(stages,[0x8B31,0x8B30]):
            shader=CreateShader(kind);code=text.encode();data=(C.c_char_p*1)(code)
            ShaderSource(shader,1,data,None);CompileShader(shader);ok=ii();GetShaderiv(shader,0x8B81,C.byref(ok))
            if not ok.value:
                log=C.create_string_buffer(65536);GetShaderInfoLog(shader,len(log),None,log)
                raise RuntimeError(str(path)+' '+str(kind)+'\n'+log.value.decode())
            compiled.append(shader)
        self.id=CreateProgram()
        for shader in compiled:AttachShader(self.id,shader)
        LinkProgram(self.id);ok=ii();GetProgramiv(self.id,0x8B82,C.byref(ok))
        for shader in compiled:DeleteShader(shader)
        if not ok.value:
            log=C.create_string_buffer(65536);GetProgramInfoLog(self.id,len(log),None,log)
            raise RuntimeError(str(path)+' link\n'+log.value.decode())
        n=ii();GetProgramiv(self.id,0x8A36,C.byref(n))
        for block in range(n.value):
            size=ii();name=C.create_string_buffer(256);GetActiveBlockiv(self.id,block,0x8A40,C.byref(size))
            GetActiveBlockName(self.id,block,256,None,name);buffer=ui();GenBuffers(1,C.byref(buffer))
            binding=0 if name.value==b'UBO' else 1
            UniformBlockBinding(self.id,block,binding)
            self.blocks.append({'name':name.value.decode(),'size':size.value,'buffer':buffer.value,'binding':binding,'fields':[]})
        GetProgramiv(self.id,0x8B86,C.byref(n))
        for index in range(n.value):
            name=C.create_string_buffer(256);count=ii();kind=ui()
            GetActiveUniform(self.id,index,256,None,C.byref(count),C.byref(kind),name)
            block=ii();offset=ii();ix=ui(index)
            GetActiveUniformsiv(self.id,1,C.byref(ix),0x8A3A,C.byref(block))
            field=name.value.decode()
            if block.value>=0:
                GetActiveUniformsiv(self.id,1,C.byref(ix),0x8A3B,C.byref(offset))
                self.blocks[block.value]['fields'].append((field.split('.')[-1],kind.value,offset.value))
            elif kind.value==0x8B5E:self.samplers.append(field)
            else:self.uniforms.append((field,kind.value))
        assert GetError()==0, 'GL reflection error'

    def bind(self,source,aliases,feedback,sizes,settings,frame):
        UseProgram(self.id)
        values=dict(settings);values.update(sizes);values['MVP']=values['MVPMatrix']=MATRIX;values['FrameCount']=frame
        values['KoOriginalSize']=values['OriginalSize'];values['KoOutputSize']=values['OutputSize']
        if self.reference:
            for block in self.blocks:
                data=bytearray(block['size'])
                for name,kind,offset in block['fields']:
                    value=values.get(name,0.0)
                    if kind==0x8B5C:struct.pack_into('<16f',data,offset,*value)
                    elif kind==0x8B52:struct.pack_into('<4f',data,offset,*value)
                    elif kind==0x1405:struct.pack_into('<I',data,offset,int(value))
                    elif kind==0x1404:struct.pack_into('<i',data,offset,int(value))
                    elif kind==0x1406:struct.pack_into('<f',data,offset,float(value))
                    else:raise AssertionError(('unexpected UBO type',name,kind))
                blob=C.create_string_buffer(bytes(data));BindBuffer(0x8A11,block['buffer'])
                BufferData(0x8A11,len(data),blob,0x88E8);BindBufferBase(0x8A11,block['binding'],block['buffer'])
        else:
            for name,kind in self.uniforms:
                loc=GetUniform(self.id,name.encode());value=values.get(name,0.0)
                if kind==0x8B5C:UniformMatrix(loc,1,0,(ff*16)(*value))
                elif kind==0x8B52:Uniform4f(loc,*value)
                elif kind==0x1404:Uniform1i(loc,int(value))
                elif kind==0x1406:Uniform1f(loc,float(value))
                else:raise AssertionError(('unexpected port uniform',name,kind))
        slots={}
        for name in self.samplers:
            if name=='Source':tex=source
            elif name in ('AGMemFeedback','FeedbackTexture'):tex=feedback
            else:
                alias=name if self.reference else name.removesuffix('Texture')
                tex=aliases[alias]
            if tex is None:raise AssertionError(('missing texture',name))
            unit=slots.setdefault(tex.id,len(slots))
            ActiveTexture(0x84C0+unit);BindTexture(0x0DE1,tex.id);Uniform1i(GetUniform(self.id,name.encode()),unit)
        enabled=[]
        for name,components,array in [('Position' if self.reference else 'VertexCoord',4,VERTEX),('TexCoord',2,UV)]:
            location=GetAttrib(self.id,name.encode())
            if location>=0:EnableAttrib(location);AttribPointer(location,components,0x1406,0,0,array);enabled.append(location)
        DrawArrays(5,0,4)
        for location in enabled:DisableAttrib(location)
        error=GetError()
        if error:raise AssertionError((self.path.name,'draw GL error',hex(error)))

    def close(self):
        for block in self.blocks:
            value=ui(block['buffer']);DeleteBuffers(1,C.byref(value))
        DeleteProgram(self.id)

class Texture:
    def __init__(self,w,h,floating=False,pixels=None,linear=False,wrap='clamp_to_edge'):
        self.w=w;self.h=h;self.floating=floating;t=ui();GenTextures(1,C.byref(t));self.id=t.value
        ActiveTexture(0x84C0);BindTexture(0x0DE1,self.id)
        blob=C.create_string_buffer(pixels) if pixels is not None else None
        TexImage(0x0DE1,0,0x881A if floating else 0x8058,w,h,0,0x1908,0x1401,blob)
        TexParameteri(0x0DE1,0x2801,0x2601 if linear else 0x2600);TexParameteri(0x0DE1,0x2800,0x2601 if linear else 0x2600)
        mode={'clamp_to_edge':0x812F,'clamp_to_border':0x812D,'repeat':0x2901,'mirrored_repeat':0x8370}[wrap]
        TexParameteri(0x0DE1,0x2802,mode);TexParameteri(0x0DE1,0x2803,mode)
        f=ui();GenFbos(1,C.byref(f));self.fbo=f.value;BindFbo(0x8D40,self.fbo)
        AttachTexture(0x8D40,0x8CE0,0x0DE1,self.id,0);assert CheckFbo(0x8D40)==0x8CD5
        if pixels is None:Viewport(0,0,w,h);ClearColor(0,0,0,0);Clear(0x4000)
        assert GetError()==0
    def update(self,pixels):
        ActiveTexture(0x84C0);BindTexture(0x0DE1,self.id);blob=C.create_string_buffer(pixels)
        TexSubImage(0x0DE1,0,0,0,self.w,self.h,0x1908,0x1401,blob)
    def read(self,floating=False):
        BindFbo(0x8D40,self.fbo);Finish()
        data=np.empty((self.h,self.w,4),dtype=np.float32 if floating else np.uint8)
        ReadPixels(0,0,self.w,self.h,0x1908,0x1406 if floating else 0x1401,data.ctypes.data_as(ptr))
        assert GetError()==0
        return data
    def close(self):
        BindFbo(0x8D40,0);a=ui(self.id);b=ui(self.fbo);DeleteTextures(1,C.byref(a));DeleteFbos(1,C.byref(b))

CACHE={}
def program_for(path,reference):
    key=(str(path),reference)
    if key not in CACHE:CACHE[key]=Program(path,reference)
    return CACHE[key]

class Pipeline:
    def __init__(self,path,reference=False,viewport=(960,720),overrides=None):
        self.path=Path(path);self.reference=reference;self.kv=read_preset(path);self.viewport=viewport
        self.count=int(self.kv['shaders']);self.programs=[];self.defaults={};self.settings={};self.feedback_index=-1
        self.targets=[];self.extra=None;self.input=None;self.old_bank=None;self.new_bank=None;self.last_frame=None
        self.alloc_size=None;self.last_outputs=[]
        for index in range(self.count):
            source=Path(self.kv['shader'+str(index)])
            self.programs.append(program_for(source,reference));self.defaults.update(defaults(source))
            if source.stem=='ag_mem':self.feedback_index=index
        if not reference:assert self.feedback_index==int(self.kv.get('feedback_pass','-1'))
        for name,(default,minimum,maximum) in self.defaults.items():
            value=float((overrides or {}).get(name,self.kv.get(name,default)))
            self.settings[name]=min(max(value,minimum),maximum)
    def allocate(self,w,h):
        for tex in self.targets:tex.close()
        if self.extra:self.extra.close()
        if self.input:self.input.close()
        self.targets=[];self.extra=None;self.alloc_size=(w,h);self.last_frame=None;self.old_bank=None;self.new_bank=None
        self.input=Texture(w,h,linear=self.kv.get('filter_linear0','false').lower() in ('true','1'),wrap=self.kv.get('wrap_mode0','clamp_to_border'))
        pw,ph=w,h
        for index in range(self.count):
            def dimension(axis,previous,view):
                kind=self.kv.get('scale_type'+str(index),self.kv.get('scale_type_'+axis+str(index),'source'))
                scale=float(self.kv.get('scale'+str(index),self.kv.get('scale_'+axis+str(index),'1')))
                return int(scale*(previous if kind=='source' else view if kind=='viewport' else 1)+0.5)
            ow=dimension('x',pw,self.viewport[0]);oh=dimension('y',ph,self.viewport[1])
            floating=self.kv.get('float_framebuffer'+str(index),'false').lower() in ('true','1')
            linear=self.kv.get('filter_linear'+str(index+1),'false').lower() in ('true','1')
            wrap=self.kv.get('wrap_mode'+str(index+1),'clamp_to_border')
            self.targets.append(Texture(ow,oh,floating,linear=linear,wrap=wrap))
            if index==self.feedback_index:self.extra=Texture(ow,oh,floating,linear=linear,wrap=wrap)
            pw,ph=ow,oh
    def render(self,image,frame):
        h,w=image.shape[:2]
        if self.alloc_size!=(w,h):self.allocate(w,h)
        self.input.update(image.tobytes());aliases={};previous=self.input;outputs=[]
        if self.feedback_index>=0 and frame!=self.last_frame:
            if self.new_bank is None:self.new_bank=self.targets[self.feedback_index];self.old_bank=self.extra
            else:self.old_bank,self.new_bank=self.new_bank,self.old_bank
        for index,program in enumerate(self.programs):
            output=self.new_bank if index==self.feedback_index else self.targets[index]
            BindFbo(0x8D40,output.fbo);Viewport(0,0,output.w,output.h);ClearColor(0,0,0,0);Clear(0x4000)
            sizes={'SourceSize':wh((previous.w,previous.h)),'OriginalSize':wh((w,h)),'OutputSize':wh((output.w,output.h))}
            sizes.update({name+'Size':wh((tex.w,tex.h)) for name,tex in aliases.items()})
            program.bind(previous,aliases,self.old_bank,sizes,self.settings,frame)
            alias=self.kv.get('alias'+str(index))
            if alias:aliases[alias]=output
            previous=output;outputs.append(output)
        self.last_frame=frame;self.last_outputs=outputs
        return previous.read()
    def close(self):
        for tex in self.targets:tex.close()
        if self.extra:self.extra.close()
        if self.input:self.input.close()
        self.targets=[];self.extra=self.input=None

def shutdown():
    for program in CACHE.values():program.close()
    CACHE.clear()
