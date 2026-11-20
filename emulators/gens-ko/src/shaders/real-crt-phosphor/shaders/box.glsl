#version 130
// Gens KO dedicated GLSL port, 2026-11-20. Original equations and pass order retained.
// This shader retains gunkun's custom terms; see ../LICENSE.txt and ../LICENSE_KR.txt.
// Real CRT Phosphor v1.4.3 - AI-assisted (Claude / ChatGPT), by gunkun. Non-commercial use free. See LICENSE.txt
// Pass 0: supersample down to the console's native grid (3D games rendered at 2x/4x get averaged = smooth polygons)
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
    // v1.4: with an -upNx preset this pass outputs 1/N size and averages N x N blocks
    float N = PRESCALE > 0.5 ? gk_pre() : gk_box(SourceSize.y);
    if (N < 1.5) { FragColor = vec4(texture(Source, vTexCoord).rgb, 1.0); return; }
    vec2 tex = floor(vTexCoord * SourceSize.xy);
    vec2 blk = floor(tex / N) * N;
    vec3 acc = vec3(0.0);
    float cnt = 0.0;
    for (int y = 0; y < 8; y++) {
        if (float(y) >= N) break;
        for (int x = 0; x < 8; x++) {
            if (float(x) >= N) break;
            vec3 c = texture(Source, (blk + vec2(float(x), float(y)) + 0.5) * SourceSize.zw).rgb;
            acc += c * c;           // average in (approx) linear light
            cnt += 1.0;
        }
    }
    FragColor = vec4(sqrt(acc / cnt), 1.0);
}

#endif
