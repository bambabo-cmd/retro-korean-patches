#version 130
// Gens KO dedicated GLSL port, 2026-11-20. Original equations and pass order retained.
// This shader retains gunkun's custom terms; see ../LICENSE.txt and ../LICENSE_KR.txt.
// Real CRT Phosphor v1.4.3 - AI-assisted (Claude / ChatGPT), by gunkun. Non-commercial use free. See LICENSE.txt
// Pass 6: phosphor afterglow memory. Keeps, per game pixel and channel, the brighter of "light now" and
// "light from the frames before, decayed". It reads its own previous output (FeedbackTexture).
// A still picture is unchanged; only places something has moved away from keep a fading glow.
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
uniform sampler2D Source;          // linear signal (pass 5)
uniform sampler2D FeedbackTexture;   // this pass, previous frame
void main()
{
    vec4 cur = texture(Source, vTexCoord);
    if (AFTERGLOW < 0.001) { FragColor = cur; return; }
    // per-frame decay so the trail LOOKS 10 % as bright after AG_TIME ms (60 frames a second).
    // The eye sees dim light brighter than it is (about light^(1/2.2)), so "looks 10 %" means
    // 0.1^2.2 = 0.6 % of the light. Red phosphor decays AG_RED times slower; alpha (beam
    // brightness) follows green/blue. (Real consumer CRTs: about 1 ms or less - this is a look.)
    float T  = max(AG_TIME, 1.0);
    float d  = pow(0.1, 2.2 * 16.6667 / T);
    float dr = pow(0.1, 2.2 * 16.6667 / (T * max(AG_RED, 1.0)));
    vec4 prev = texture(FeedbackTexture, vTexCoord);
    FragColor = max(cur, prev * vec4(dr, d, d, d));
}

#endif
