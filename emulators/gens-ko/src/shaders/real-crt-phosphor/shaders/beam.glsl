#version 130
// Gens KO dedicated GLSL port, 2026-11-20. Original equations and pass order retained.
// This shader retains gunkun's custom terms; see ../LICENSE.txt and ../LICENSE_KR.txt.
// Real CRT Phosphor v1.4.3 - AI-assisted (Claude / ChatGPT), by gunkun. Non-commercial use free. See LICENSE.txt
// Pass 10: scanline beams + glow, linear light at screen size (no mask yet)
uniform vec4 SourceSize;
uniform vec4 KoOriginalSize;
uniform vec4 KoOutputSize;
uniform int FrameCount;
uniform vec4 SignalSize;
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
uniform sampler2D Source;   // glow
uniform sampler2D SignalTexture;   // graded signal in linear light, alpha = linear ungraded brightness
void main()
{
    // v1.3: where this screen pixel lands on the curved tube (identity when flat)
    vec2 uv = gk_warp(vTexCoord, KoOutputSize.xy);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    vec4 SS = SignalSize;
    float N = gk_box(SS.y);
    float R = gk_rows(SS.y);
    float Hc = SS.y / N;
    float lines = Hc / R;
    float nW = SS.x / N;
    float ny = uv.y * lines;
    float rowHere = floor(uv.y * Hc);
    float nx = uv.x * nW - 0.5;
    float x0 = floor(nx);
    float fx = nx - x0;
    float base = floor(ny - 0.5);
    vec3 acc = vec3(0.0);
    for (int k = -1; k <= 2; k++) {
        float li = clamp(base + float(k), 0.0, lines - 1.0);
        float d  = abs(ny - (base + float(k) + 0.5));
        float row = clamp(rowHere, li * R, li * R + R - 1.0);
        float yT = (row + 0.5) * N * SS.w;
        vec4 sA = texture(SignalTexture, vec2((clamp(x0,       0.0, nW - 1.0) + 0.5) * N * SS.z, yT));
        vec4 sB = texture(SignalTexture, vec2((clamp(x0 + 1.0, 0.0, nW - 1.0) + 0.5) * N * SS.z, yT));
        vec3 col = mix(sA.rgb, sB.rgb, fx);          // v1.1.1: SignalTexture is stored linear
        float lu = mix(sA.a, sB.a, fx);
        float w = BEAM_DARK + (BEAM_BRIGHT - BEAM_DARK) * sqrt(clamp(lu, 0.0, 1.0));
        acc += col * exp(-0.5 * (d / w) * (d / w)) / (w * 2.5066);
    }
    acc += GLOW * texture(Source, uv).rgb;
    FragColor = vec4(acc, 1.0);
}

#endif
