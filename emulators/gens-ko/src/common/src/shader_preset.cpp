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

KoShaderPass::KoShaderPass() : wrap("clamp_to_border"), scale_type_x("source"),
 scale_type_y("source"), scale_x(1.0), scale_y(1.0), linear(false), frame_mod(0) {}

static std::string trim(const std::string &s) {
    std::string::size_type a=s.find_first_not_of(" \t\r\n"), b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos ? std::string() : s.substr(a,b-a+1);
}
static bool number(const std::string &s, double &v) {
    errno=0; char *end=0; v=strtod(s.c_str(), &end);
    return !s.empty() && end && *end==0 && !errno && v==v &&
           v<=std::numeric_limits<double>::max() && v>=-std::numeric_limits<double>::max();
}
static bool boolean(const std::string &s, bool &v) {
    if(s=="true"||s=="1") {v=true;return true;}
    if(s=="false"||s=="0") {v=false;return true;} return false;
}
static std::string idx(unsigned int n) {std::ostringstream ss;ss<<n;return ss.str();}

bool KoParseShaderPreset(const std::string &source, KoShaderPreset &out, std::string &error) {
    std::string text=source;
    if(text.compare(0,3,"\xef\xbb\xbf")==0) text.erase(0,3);
    if(text.size()>1024*1024) {error="Preset exceeds the 1 MiB limit.";return false;}
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
    if(!number(kv["shaders"],count)||count<1||count>8||floor(count)!=count) {
        error="This renderer supports 1 to 8 GLSL passes.";return false;
    }
    if(!kv["textures"].empty()) {error="Lookup textures (LUT) are not supported in this build.";return false;}
    if(kv.count("feedback_pass")) {error="Feedback framebuffers are not supported in this build.";return false;}
    KoShaderPreset candidate;
    for(unsigned int i=0;i<(unsigned int)count;++i) {
        KoShaderPass p; std::string n=idx(i);
        if(!kv.count("shader"+n)||kv["shader"+n].empty()) {error="Missing shader"+n;return false;}
        p.file=kv["shader"+n]; p.alias=kv["alias"+n];
        if(!p.alias.empty()) {
            for(size_t j=0;j<p.alias.size();++j) if(!(isalnum((unsigned char)p.alias[j])||p.alias[j]=='_')) {
                error="Shader alias must be a GLSL identifier.";return false;
            }
            if(isdigit((unsigned char)p.alias[0])) {error="Shader alias cannot start with a digit.";return false;}
        }
        const char *unsupported[]={"float_framebuffer","srgb_framebuffer","mipmap_input"};
        for(unsigned int k=0;k<3;++k) if(kv.count(std::string(unsupported[k])+n)) {
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
    std::istringstream plist(kv["parameters"]); std::string param;
    while(std::getline(plist,param,';')) {
        param=trim(param); if(param.empty())continue;
        if(!kv.count(param))continue;
        double value=0;
        if(!number(kv[param],value)||fabs(value)>1e20) {error="Invalid shader parameter: "+param;return false;}
        candidate.parameters[param]=(float)value;
    }
    out=candidate;error.clear();return true;
}
