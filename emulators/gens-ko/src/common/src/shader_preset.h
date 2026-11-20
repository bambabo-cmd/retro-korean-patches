/* Gens KO GLSL preset reader, 2026-11-20. GPL-2.0; see LICENSE.txt. */
#ifndef GENS_KO_SHADER_PRESET_H
#define GENS_KO_SHADER_PRESET_H
#include <string>
#include <vector>
#include <map>

static const unsigned int KO_SHADER_MAX_PASSES = 16;

struct KoShaderPass {
    std::string file, alias, wrap, scale_type_x, scale_type_y;
    double scale_x, scale_y;
    bool linear, float_framebuffer;
    unsigned int frame_mod;
    KoShaderPass();
};
struct KoShaderPreset {
    std::vector<KoShaderPass> passes;
    std::map<std::string, float> parameters;
    /* Zero-based pass whose preceding output frame is sampled as feedback.
     * -1 means no feedback input. It is never inferred from a sampler name. */
    int feedback_pass;
    KoShaderPreset() : feedback_pass(-1) {}
};
/* No file I/O or graphics dependency: malformed presets fail before GPU work. */
bool KoParseShaderPreset(const std::string &text, KoShaderPreset &result,
                         std::string &error);
#endif
