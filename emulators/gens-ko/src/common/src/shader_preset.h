/* Gens KO GLSL preset reader, 2026-11-20. GPL-2.0; see LICENSE.txt. */
#ifndef GENS_KO_SHADER_PRESET_H
#define GENS_KO_SHADER_PRESET_H
#include <string>
#include <vector>
#include <map>

struct KoShaderPass {
    std::string file, alias, wrap, scale_type_x, scale_type_y;
    double scale_x, scale_y;
    bool linear;
    unsigned int frame_mod;
    KoShaderPass();
};
struct KoShaderPreset {
    std::vector<KoShaderPass> passes;
    std::map<std::string, float> parameters;
};
/* No file I/O or graphics dependency: malformed presets fail before GPU work. */
bool KoParseShaderPreset(const std::string &text, KoShaderPreset &result,
                         std::string &error);
#endif
