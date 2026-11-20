/* GLSLP format regression cases. GPL-2.0, 2026-11-20. */
#include "../src/common/src/shader_preset.h"
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
static unsigned checks=0;
static KoShaderPreset parse(const std::string &s) {
    KoShaderPreset p;std::string e="old error";if(!KoParseShaderPreset(s,p,e)){std::cerr<<e<<"\n";assert(false);}
    assert(e.empty());++checks;return p;
}
static bool equalPass(const KoShaderPass &a,const KoShaderPass &b) {
    return a.file==b.file && a.alias==b.alias && a.wrap==b.wrap && a.scale_type_x==b.scale_type_x &&
        a.scale_type_y==b.scale_type_y && a.scale_x==b.scale_x && a.scale_y==b.scale_y &&
        a.linear==b.linear && a.float_framebuffer==b.float_framebuffer && a.frame_mod==b.frame_mod;
}
static void reject(const std::string &s,const std::string &reason="") {
    KoShaderPreset p;p.passes.resize(2);p.passes[0].file="preserved";p.passes[1].file="other.glsl";
    p.passes[0].alias="SavedAlias";p.passes[0].float_framebuffer=true;p.passes[1].frame_mod=19;
    p.feedback_pass=1;p.parameters["EXISTING"]=2.75f;
    KoShaderPreset before=p;
    std::string e;bool accepted=KoParseShaderPreset(s,p,e);
    if(accepted || e.empty() || (!reason.empty() && e.find(reason)==std::string::npos)) {
        std::cerr<<"Unexpected rejection outcome: accepted="<<accepted<<" error="<<e<<" expected="<<reason<<"\n";
        assert(false);
    }
    assert(p.passes.size()==before.passes.size() && p.parameters==before.parameters && p.feedback_pass==before.feedback_pass);
    for(size_t i=0;i<p.passes.size();++i) assert(equalPass(p.passes[i],before.passes[i]));
    ++checks;
}
static std::string graph(unsigned int count) {
    std::ostringstream text;text<<"shaders="<<count<<"\n";
    for(unsigned int i=0;i<count;++i) text<<"shader"<<i<<"=pass"<<i<<".glsl\n";
    return text.str();
}
int main(int argc,char **argv) {
    KoShaderPreset p=parse("shaders=2\nshader0=first.glsl\nscale_type0=source\nscale0=4.0\nfilter_linear0=false\nshader1=last.glsl\nfilter_linear1=true\n");
    assert(p.passes[0].scale_x==4&&p.passes[0].scale_type_x=="source");
    assert(p.passes[1].scale_type_x=="viewport"&&p.passes[1].linear);
    p=parse("shaders=1\nshader0=\"with spaces #hash.glsl\" #comment\nscale_type_x0=absolute\nscale_x0=640\nscale_type_y0=viewport\nscale_y0=0.5\n");
    assert(p.passes[0].file=="with spaces #hash.glsl");
    assert(p.passes[0].scale_type_x=="absolute"&&p.passes[0].scale_x==640);
    assert(p.passes[0].scale_type_y=="viewport"&&p.passes[0].scale_y==0.5);
    p=parse("shaders=1\nshader0=x.glsl\nscale_type0=source\nscale_type_x0=absolute\nscale0=2\nscale_x0=100\nparameters=\"POWER;MASK\"\nPOWER=2.2\nMASK=0.3\nframe_count_mod0=60\n");
    assert(p.passes[0].scale_type_x=="source"&&p.passes[0].scale_x==2);
    assert(p.parameters.size()==2&&p.passes[0].frame_mod==60);
    p=parse("\xef\xbb\xbfshaders=1\r\nshader0=x.glsl\r\nfloat_framebuffer0=false\r\nsrgb_framebuffer0=0\r\n");
    assert(!p.passes[0].float_framebuffer && p.feedback_pass==-1);
    // The original CRT chain has 14 passes; its GBA variant has 15. All valid
    // indices, including the final output, can be the previous-frame source.
    for(unsigned int count=1;count<=KO_SHADER_MAX_PASSES;++count) {
        p=parse(graph(count));assert(p.passes.size()==count && p.feedback_pass==-1);
        for(unsigned int feedback=0;feedback<count;++feedback) {
            std::ostringstream extra;extra<<"feedback_pass="<<feedback<<"\nfloat_framebuffer"<<feedback<<"=true\n";
            p=parse(graph(count)+extra.str());assert(p.feedback_pass==(int)feedback);
            for(unsigned int i=0;i<count;++i) assert(p.passes[i].float_framebuffer==(i==feedback));
        }
    }
    p=parse(graph(15)+"float_framebuffer3=1\nfloat_framebuffer14=0\nfeedback_pass=7\n");
    assert(p.passes[3].float_framebuffer && !p.passes[14].float_framebuffer && p.feedback_pass==7);
    reject("shaders=0\n","1 to 16");reject("shaders=17\n","1 to 16");reject("shaders=1.5\n");
    reject("shaders=1\n");reject("shaders=1\nshader0=x\nshader0=y\n");
    reject("shaders=1\nshader0=\"unfinished\n");
    reject("shaders=1\nshader0=x\ntextures=mask\nmask=x.png\n");
    reject("shaders=1\nshader0=x\nfeedback_pass=-1\n","feedback_pass");
    reject("shaders=1\nshader0=x\nfeedback_pass=1\n","feedback_pass");
    reject("shaders=2\nshader0=x\nshader1=y\nfeedback_pass=0.5\n","feedback_pass");
    reject("shaders=1\nshader0=x\nfloat_framebuffer0=maybe\n","float_framebuffer0");
    reject("shaders=1\nshader0=x\nmipmap_input0=true\n");
    reject("shaders=1\nshader0=x\nsrgb_framebuffer0=true\n","srgb_framebuffer0");
    reject("#reference \"base.glslp\"\nshaders=1\nshader0=x\n");
    reject("shaders=1\nshader0=x\nscale0=NaN\n");
    reject("shaders=1\nshader0=x\nscale0=1e300\n");
    reject("shaders=1\nshader0=x\nscale_type0=mystery\n");
    reject("shaders=1\nshader0=x\nfilter_linear0=maybe\n");
    reject("shaders=1\nshader0=x\nframe_count_mod0=-1\n");
    const char *nonfinite[]={"NaN","nan","-nan","inf","Infinity","-inf","1e9999","1.0 trailing"};
    for(size_t i=0;i<sizeof(nonfinite)/sizeof(nonfinite[0]);++i) {
        std::string value(nonfinite[i]);
        reject(graph(1)+"feedback_pass="+value+"\n","feedback_pass");
        reject(graph(1)+"scale0="+value+"\n","scale0");
        reject(graph(1)+"parameters=GAIN\nGAIN="+value+"\n","parameter");
        reject(graph(1)+"frame_count_mod0="+value+"\n","frame_count_mod");
    }
    reject(graph(1)+"scale0=0\n","range");reject(graph(1)+"scale0=-1\n","range");
    reject(graph(1)+"scale0=8192.0001\n","range");p=parse(graph(1)+"scale0=8192\n");assert(p.passes[0].scale_x==8192);
    reject(graph(1)+"frame_count_mod0=2147483648\n","frame_count_mod");
    p=parse(graph(1)+"frame_count_mod0=2147483647\n");assert(p.passes[0].frame_mod==2147483647u);
    reject(graph(1)+"feedback_pass=0\nfeedback_pass=0\n","Duplicate preset key");
    reject(graph(1)+"float_framebuffer0=true\nfloat_framebuffer0=false\n","Duplicate preset key");
    const char *badkeys[]={"shader1","shader-1","shader00","shader99999999999999999999",
        "float_framebuffer1","float_framebuffer-1","float_framebuffer00","srgb_framebuffer1",
        "mipmap_input1","alias1","filter_linear1","scale_type_x1","scale_x1","scale1","scale"};
    for(size_t i=0;i<sizeof(badkeys)/sizeof(badkeys[0]);++i) reject(graph(1)+badkeys[i]+"=true\n","pass");
    p=parse(graph(2)+"alias0=Box\nalias1=box\n");assert(p.passes[0].alias!=p.passes[1].alias);
    reject(graph(2)+"alias0=Kern\nalias1=Kern\n","Duplicate shader alias");
    const char *reserved[]={"Orig","Original","Source","Feedback","Feedback0","Pass1","Pass16","PassPrev1",
        "Prev","Prev3","Texture","Input","Output","KoOriginal","KoOutput","gl_name","name__reserved","3bad","bad-name"};
    for(size_t i=0;i<sizeof(reserved)/sizeof(reserved[0]);++i) reject(graph(1)+"alias0="+reserved[i]+"\n","alias");
    reject(graph(1)+"alias0="+std::string(129,'a')+"\n","alias");
    reject(graph(1)+"alias0=\xc3\xa9\n","alias");
    p=parse(graph(1)+"alias0=_Valid123\n");assert(p.passes[0].alias=="_Valid123");
    reject(graph(1)+"parameters=GAIN;GAIN\nGAIN=0.2\n","Duplicate shader parameter");
    const char *reservedParams[]={"Texture","FeedbackTexture","Pass1Size","PassPrev2Texture","OrigTexture",
        "SourceSize","KoOriginalSize","FrameCount","MVPMatrix","shader0","1bad","bad-name","gl_name"};
    for(size_t i=0;i<sizeof(reservedParams)/sizeof(reservedParams[0]);++i)
        reject(graph(1)+"parameters="+reservedParams[i]+"\n","parameter");
    reject(graph(1)+"alias0=Kern\nparameters=KernTexture\nKernTexture=1\n","parameter");
    reject(graph(1)+"parameters=GAIN\nGAIN=1e21\n","parameter");
    p=parse(graph(1)+"parameters=GAIN;DEFAULT_ONLY\nGAIN=-1e20\n");assert(p.parameters.size()==1 && p.parameters["GAIN"]<0);
    // A successful replacement must clear all prior per-pass/feedback/options.
    KoShaderPreset reused=parse(graph(3)+"feedback_pass=2\nfloat_framebuffer2=true\nparameters=GAIN\nGAIN=2\n");
    std::string e;assert(KoParseShaderPreset(graph(1),reused,e));++checks;
    assert(reused.passes.size()==1 && reused.feedback_pass==-1 && reused.parameters.empty() && !reused.passes[0].float_framebuffer);
    std::string embedded=graph(1)+"scale0=1";embedded.push_back('\0');embedded+="ignored\n";
    reject(embedded,"NUL");reject(std::string(1024*1024+1,'#'),"1 MiB");
    for(int i=1;i<argc;++i){std::ifstream file(argv[i]);assert(file);std::ostringstream text;text<<file.rdbuf();parse(text.str());}
    std::cout<<"GLSLP format cases passed: "<<checks<<"\n";return 0;
}
