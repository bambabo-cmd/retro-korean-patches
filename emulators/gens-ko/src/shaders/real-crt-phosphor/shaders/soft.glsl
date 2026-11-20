#version 130
// Gens KO dedicated GLSL port, 2026-11-20. Original equations and pass order retained.
// This shader retains gunkun's custom terms; see ../LICENSE.txt and ../LICENSE_KR.txt.
// Real CRT Phosphor v1.4.3 - AI-assisted (Claude / ChatGPT), by gunkun. Non-commercial use free. See LICENSE.txt
// Pass 2: small blur in every direction (up/down/left/right at once), linear light, on the native pixel grid.
// Balances the rightward analog smear so softening does not lean to one side.
uniform vec4 SourceSize;
uniform vec4 KoOriginalSize;
uniform vec4 KoOutputSize;
uniform int FrameCount;
#include "params.inc"
#include "ubo.inc"
#include "common.inc"
#if defined(VERTEX)
in vec4 VertexCoord;
in vec2 TexCoord;
out vec2 vTexCoord;
void main() { gl_Position = MVPMatrix * VertexCoord; vTexCoord = TexCoord; }
#elif defined(FRAGMENT)
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D Source;
void main()
{
    float sa = SOFT_ALL;
    if (sa < 0.01) { FragColor = vec4(texture(Source, vTexCoord).rgb, 1.0); return; }
    float N = gk_box(SourceSize.y);
    vec2 st = N * SourceSize.zw;          // one content pixel in each direction
    vec3 acc = vec3(0.0); float ws = 0.0;
    for (int y = -2; y <= 2; y++) {
        for (int x = -2; x <= 2; x++) {
            float w = exp(-0.5 * float(x * x + y * y) / (sa * sa));
            vec3 c = texture(Source, vTexCoord + vec2(float(x), float(y)) * st).rgb;
            acc += w * c * c;                     // approx. linear light (gamma 2)
            ws += w;
        }
    }
    FragColor = vec4(sqrt(acc / ws), 1.0);
}

#endif
