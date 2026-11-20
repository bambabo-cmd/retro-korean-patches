#version 130
// Gens KO dedicated GLSL port, 2026-11-20. Original equations and pass order retained.
// This shader retains gunkun's custom terms; see ../LICENSE.txt and ../LICENSE_KR.txt.
// Real CRT Phosphor v1.4.3 - AI-assisted (Claude / ChatGPT), by gunkun. Non-commercial use free. See LICENSE.txt
// Pass 7: phosphor afterglow output. Puts the remembered glow, scaled by the strength, under the current picture.
// max(now, strength x memory): still pictures stay exactly as they were.
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
uniform sampler2D Source;   // afterglow memory (pass 6)
uniform sampler2D SigRawTexture;   // linear signal without afterglow (pass 5)
void main()
{
    vec4 cur = texture(SigRawTexture, vTexCoord);
    float s = AFTERGLOW;
    if (s < 0.001) { FragColor = cur; return; }
    FragColor = max(cur, texture(Source, vTexCoord) * s);
}

#endif
