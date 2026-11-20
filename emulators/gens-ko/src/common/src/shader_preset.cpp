/* Gens KO GLSL preset reader, 2026-11-20. GPL-2.0; see LICENSE.txt.
   Independently implemented from the public Libretro shader format docs.
   No RetroArch or third-party shader implementation is incorporated here. */
#include "shader_preset.h"
#include <sstream>
#include <cstdlib>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>

KoShaderPass::KoShaderPass() : wrap("clamp_to_border"), scale_type_x("source"),
 scale_type_y("source"), scale_x(1.0), scale_y(1.0), linear(false),
 float_framebuffer(false), frame_mod(0) {}

static std::string trim(const std::string &s) {
    std::string::size_type a=s.find_first_not_of(" \t\r\n"), b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos ? std::string() : s.substr(a,b-a+1);
}
static bool number(const std::string &s, double &v) {
    errno=0; char *end=0; v=strtod(s.c_str(), &end);
    return !s.empty() && end==s.c_str()+s.size() && !errno && v==v &&
           v<=std::numeric_limits<double>::max() && v>=-std::numeric_limits<double>::max();
}
static bool boolean(const std::string &s, bool &v) {
    if(s=="true"||s=="1") {v=true;return true;}
    if(s=="false"||s=="0") {v=false;return true;} return false;
}
static std::string idx(unsigned int n) {std::ostringstream ss;ss<<n;return ss.str();}

static bool identifier(const std::string &name) {
    if(name.empty() || name.size()>128) return false;
    for(size_t i=0;i<name.size();++i) {
        unsigned char c=(unsigned char)name[i];
        bool letter=(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_';
        if(!letter && !(i && c>='0'&&c<='9')) return false;
    }
    return name.compare(0,3,"gl_")!=0 && name.find("__")==std::string::npos;
}
static bool numberedName(const std::string &name,const char *base) {
    std::string prefix(base);
    if(name.compare(0,prefix.size(),prefix)!=0) return false;
    for(size_t i=prefix.size();i<name.size();++i) if(name[i]<'0'||name[i]>'9') return false;
    return true; /* The bare prefix is reserved as well. */
}
static bool reservedAlias(const std::string &name) {
    const char *reserved[]={"Orig","Original","Source","Texture","Input","Output",
        "Feedback","KoOriginal","KoOutput"};
    for(size_t i=0;i<sizeof(reserved)/sizeof(reserved[0]);++i) if(name==reserved[i]) return true;
    return numberedName(name,"Pass") || numberedName(name,"PassPrev") ||
           name.compare(0,4,"Prev")==0 || numberedName(name,"Feedback");
}
/* These options take a canonical zero-based suffix. Silently ignoring, for
 * example, float_framebuffer16 or shader-1 would change the requested graph. */
static const char *passProperties[]={
    "float_framebuffer","srgb_framebuffer","frame_count_mod","filter_linear",
    "mipmap_input","scale_type_x","scale_type_y","scale_type","scale_x","scale_y",
    "wrap_mode","shader","alias","scale"
};
static bool validatePassKey(const std::string &key,unsigned int count,std::string &error) {
    if(key=="shaders") return true;
    for(size_t i=0;i<sizeof(passProperties)/sizeof(passProperties[0]);++i) {
        std::string prefix(passProperties[i]);
        if(key.compare(0,prefix.size(),prefix)!=0) continue;
        std::string suffix=key.substr(prefix.size());unsigned int pass=0;
        if(suffix.empty() || (suffix.size()>1 && suffix[0]=='0')) {
            error="Invalid pass index in preset key: "+key;return false;
        }
        for(size_t j=0;j<suffix.size();++j) {
            if(suffix[j]<'0'||suffix[j]>'9') {error="Invalid pass index in preset key: "+key;return false;}
            pass=pass*10+(unsigned int)(suffix[j]-'0');
            if(pass>=count) {error="Preset key refers to a nonexistent pass: "+key;return false;}
        }
        return true;
    }
    return true;
}
static bool reservedParameter(const std::string &name) {
    const char *reserved[]={"shaders","parameters","textures","feedback_pass",
        "Texture","Source","InputSize","TextureSize","OutputSize","MVPMatrix",
        "FrameCount","FrameDirection","SourceSize","KoOriginalSize","KoOutputSize"};
    for(size_t i=0;i<sizeof(reserved)/sizeof(reserved[0]);++i) if(name==reserved[i]) return true;
    for(size_t i=0;i<sizeof(passProperties)/sizeof(passProperties[0]);++i)
        if(name.compare(0,std::string(passProperties[i]).size(),passProperties[i])==0) return true;
    return false;
}
static bool generatedUniform(const std::string &name,const std::string &prefix) {
    const char *suffixes[]={"Texture","InputSize","TextureSize","Size","TexCoord"};
    for(size_t i=0;i<sizeof(suffixes)/sizeof(suffixes[0]);++i) if(name==prefix+suffixes[i]) return true;
    return false;
}
static bool parameterConflictsWithInput(const std::string &name,const std::set<std::string> &aliases) {
    const char *prefixes[]={"Orig","Original","Source","Feedback","Prev"};
    for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]);++i) if(generatedUniform(name,prefixes[i])) return true;
    for(unsigned int i=0;i<=KO_SHADER_MAX_PASSES;++i)
        if(generatedUniform(name,"Pass"+idx(i)) || generatedUniform(name,"PassPrev"+idx(i)) ||
           generatedUniform(name,"Prev"+idx(i))) return true;
    for(std::set<std::string>::const_iterator it=aliases.begin();it!=aliases.end();++it)
        if(generatedUniform(name,*it)) return true;
    return false;
}

bool KoParseShaderPreset(const std::string &source, KoShaderPreset &out, std::string &error) {
    std::string text=source;
    if(text.compare(0,3,"\xef\xbb\xbf")==0) text.erase(0,3);
    if(text.size()>1024*1024) {error="Preset exceeds the 1 MiB limit.";return false;}
    if(text.find('\0')!=std::string::npos) {error="Preset contains an embedded NUL byte.";return false;}
    std::map<std::string,std::string> kv;
    std::istringstream lines(text); std::string line;
    while(std::getline(lines,line)) {
        line=trim(line); if(line.empty())continue;
        if(line.compare(0,10,"#reference")==0||line.compare(0,8,"#include")==0) {
            error="Inherited/include presets are not supported; load the full GLSL preset.";return false;
        }
        if(line[0]=='#')continue;
        bool quoted=false; size_t eq=std::string::npos;
        for(size_t i=0;i<line.size();++i) {
            if(line[i]=='\"'&&(i==0||line[i-1]!='\\'))quoted=!quoted;
            if(!quoted&&line[i]=='='&&eq==std::string::npos)eq=i;
            if(!quoted&&line[i]=='#') {line.erase(i);break;}
        }
        if(quoted||eq==std::string::npos) {error="Malformed preset assignment or quotation.";return false;}
        std::string key=trim(line.substr(0,eq)), value=trim(line.substr(eq+1));
        if(key.empty()||value.empty()) {error="Empty preset key or value.";return false;}
        if(value[0]=='\"') {
            if(value.size()<2||value[value.size()-1]!='\"') {error="Malformed quoted value.";return false;}
            value=value.substr(1,value.size()-2);
        }
        if(kv.count(key)) {error="Duplicate preset key: "+key;return false;}
        kv[key]=value;
    }
    double count=0;
    if(!number(kv["shaders"],count)||count<1||count>KO_SHADER_MAX_PASSES||floor(count)!=count) {
        error="This renderer supports 1 to 16 GLSL passes.";return false;
    }
    if(!kv["textures"].empty()) {error="Lookup textures (LUT) are not supported in this build.";return false;}
    KoShaderPreset candidate;
    if(kv.count("feedback_pass")) {
        double feedback=0;
        if(!number(kv["feedback_pass"],feedback)||feedback<0||feedback>=count||floor(feedback)!=feedback) {
            error="feedback_pass must select a zero-based pass from 0 to shaders-1.";return false;
        }
        candidate.feedback_pass=(int)feedback;
    }
    for(std::map<std::string,std::string>::const_iterator it=kv.begin();it!=kv.end();++it)
        if(!validatePassKey(it->first,(unsigned int)count,error)) return false;
    std::set<std::string> aliases;
    for(unsigned int i=0;i<(unsigned int)count;++i) {
        KoShaderPass p; std::string n=idx(i);
        if(!kv.count("shader"+n)||kv["shader"+n].empty()) {error="Missing shader"+n;return false;}
        p.file=kv["shader"+n]; p.alias=kv["alias"+n];
        if(!p.alias.empty()) {
            if(!identifier(p.alias)) {error="Shader alias must be a nonreserved ASCII GLSL identifier of at most 128 characters.";return false;}
            if(reservedAlias(p.alias)) {error="Shader alias conflicts with a renderer input: "+p.alias;return false;}
            if(!aliases.insert(p.alias).second) {error="Duplicate shader alias: "+p.alias;return false;}
        }
        if(kv.count("float_framebuffer"+n)&&!boolean(kv["float_framebuffer"+n],p.float_framebuffer)) {
            error="Invalid float_framebuffer"+n;return false;
        }
        const char *unsupported[]={"srgb_framebuffer","mipmap_input"};
        for(unsigned int k=0;k<2;++k) if(kv.count(std::string(unsupported[k])+n)) {
            bool enabled=false;
            if(!boolean(kv[std::string(unsupported[k])+n],enabled)||enabled) {
                error=std::string("Unsupported preset option: ")+unsupported[k]+n;return false;
            }
        }
        if(kv.count("filter_linear"+n)&&!boolean(kv["filter_linear"+n],p.linear)) {
            error="Invalid filter_linear"+n;return false;
        }
        if(kv.count("wrap_mode"+n))p.wrap=kv["wrap_mode"+n];
        if(p.wrap!="clamp_to_border"&&p.wrap!="clamp_to_edge"&&p.wrap!="repeat"&&p.wrap!="mirrored_repeat") {
            error="Unsupported wrap_mode"+n;return false;
        }
        bool hasScale=kv.count("scale"+n)||kv.count("scale_x"+n)||kv.count("scale_y"+n)||
          kv.count("scale_type"+n)||kv.count("scale_type_x"+n)||kv.count("scale_type_y"+n);
        if(i+1==(unsigned int)count&&!hasScale)p.scale_type_x=p.scale_type_y="viewport";
        if(kv.count("scale_type_x"+n))p.scale_type_x=kv["scale_type_x"+n];
        if(kv.count("scale_type_y"+n))p.scale_type_y=kv["scale_type_y"+n];
        if(kv.count("scale_type"+n))p.scale_type_x=p.scale_type_y=kv["scale_type"+n];
        if((p.scale_type_x!="source"&&p.scale_type_x!="viewport"&&p.scale_type_x!="absolute")||
           (p.scale_type_y!="source"&&p.scale_type_y!="viewport"&&p.scale_type_y!="absolute")) {
            error="Invalid scale type in pass "+n;return false;
        }
        if(kv.count("scale_x"+n)&&!number(kv["scale_x"+n],p.scale_x)) {error="Invalid scale_x"+n;return false;}
        if(kv.count("scale_y"+n)&&!number(kv["scale_y"+n],p.scale_y)) {error="Invalid scale_y"+n;return false;}
        if(kv.count("scale"+n)) {
            if(!number(kv["scale"+n],p.scale_x)) {error="Invalid scale"+n;return false;}
            p.scale_y=p.scale_x;
        }
        if(p.scale_x<=0||p.scale_y<=0||p.scale_x>8192||p.scale_y>8192) {error="Scale outside the supported range.";return false;}
        if(kv.count("frame_count_mod"+n)) {
            double v=0;
            if(!number(kv["frame_count_mod"+n],v)||v<=0||v>2147483647.0||floor(v)!=v) {error="Invalid frame_count_mod"+n;return false;}
            p.frame_mod=(unsigned int)v;
        }
        candidate.passes.push_back(p);
    }
    std::istringstream plist(kv["parameters"]); std::string param;std::set<std::string> listed;
    while(std::getline(plist,param,';')) {
        param=trim(param); if(param.empty())continue;
        if(!identifier(param)||reservedParameter(param)||parameterConflictsWithInput(param,aliases)) {
            error="Invalid or reserved shader parameter: "+param;return false;
        }
        if(!listed.insert(param).second) {error="Duplicate shader parameter: "+param;return false;}
        if(!kv.count(param))continue;
        double value=0;
        if(!number(kv[param],value)||fabs(value)>1e20) {error="Invalid shader parameter: "+param;return false;}
        candidate.parameters[param]=(float)value;
    }
    /* All validation and allocations finish before committing a new graph. */
    out.passes.swap(candidate.passes);out.parameters.swap(candidate.parameters);
    out.feedback_pass=candidate.feedback_pass;error.clear();return true;
}
