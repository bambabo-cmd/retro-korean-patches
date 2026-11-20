#version 130
// Gens KO dedicated GLSL port, 2026-11-20. Original equations and pass order retained.
// This shader retains gunkun's custom terms; see ../LICENSE.txt and ../LICENSE_KR.txt.
// Real CRT Phosphor v1.4.3 - AI-assisted (Claude / ChatGPT), by gunkun. Non-commercial use free. See LICENSE.txt
// Pass 8: glow, horizontal blur (linear light)
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
    float N  = gk_box(SourceSize.y);
    float nW = SourceSize.x / N;
    float k  = gk_kg(nW, SourceSize.y);   // v1.4.1/v1.4.2: same glow size on screen for 256 and 512 modes
    vec2 stp = vec2(N * SourceSize.z, 0.0);
    float s = max(GLOW_R * 0.875 * k, 0.1);
    int r = min(int(ceil(3.0 * s)), 24);
    vec3 acc = vec3(0.0); float ws = 0.0;
    for (int j = -24; j <= 24; j++) {
        if (j < -r) continue;
        if (j > r) break;
        float t = float(j);
        float w = exp(-0.5 * t * t / (s * s));
        acc += w * texture(Source, vTexCoord + t * stp).rgb;   // v1.1.1: Signal is already linear
        ws += w;
    }
    FragColor = vec4(acc / ws, 1.0);
}

#endif
