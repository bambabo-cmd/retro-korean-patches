#version 130
// Gens KO dedicated GLSL port, 2026-11-20. Original equations and pass order retained.
// This shader retains gunkun's custom terms; see ../LICENSE.txt and ../LICENSE_KR.txt.
// Real CRT Phosphor v1.4.3 - AI-assisted (Claude / ChatGPT), by gunkun. Non-commercial use free. See LICENSE.txt
// Pass 3: lookup row of the horizontal filter weights (only the first row / first 128 texels are used).
// The weights are the same for every pixel, so they are computed once here instead of per pixel in the signal pass.
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
    vec2 tx = floor(vTexCoord * KoOutputSize.xy);
    if (tx.y > 0.5 || tx.x > 127.5) { FragColor = vec4(0.0); return; }   // only row 0, texels 0..127 are needed
    float N  = gk_box(SourceSize.y);
    float nW = SourceSize.x / N;
    float t  = tx.x - 32.0;                                  // tap offset -32 .. +95
    if (gk_wide(nW, SourceSize.y)) {
        // v1.4.1: wide modes use the 256 filter, stretched by nW/256 with linear interpolation between its taps
        float u  = t * 256.0 / nW;
        float u0 = floor(u), f = u - u0;
        float sY  = LUMA_BLUR;
        float tY  = LUMA_TAIL;
        float sY2 = sqrt(sY * sY + 1.44);
        float sC  = CHROMA_BLUR;
        float tC  = CHROMA_TAIL;
        vec3 a = vec3(gk_exgauss(u0, sY, tY), gk_exgauss(u0, sY2, tY), gk_exgauss(u0, sC, tC));
        vec3 b = vec3(gk_exgauss(u0 + 1.0, sY, tY), gk_exgauss(u0 + 1.0, sY2, tY), gk_exgauss(u0 + 1.0, sC, tC));
        // between two 256 taps the weight is lowered to 70 %, so the stretched filter is not wider than needed
        FragColor = vec4(mix(a, b, f) * (1.0 - 0.3 * 4.0 * f * (1.0 - f)), 1.0);
        return;
    }
    float k  = gk_k(nW);
    float sY  = LUMA_BLUR * k;
    float tY  = LUMA_TAIL * k;
    float sY2 = sqrt(sY * sY + 1.44 * k * k);
    float sC  = CHROMA_BLUR * k;
    float tC  = CHROMA_TAIL * k;
    FragColor = vec4(gk_exgauss(t, sY, tY), gk_exgauss(t, sY2, tY), gk_exgauss(t, sC, tC), 1.0);
}

#endif
