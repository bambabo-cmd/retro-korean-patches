#version 120
// Gens KO sample shader, 2026-11-20. GPL-2.0; see LICENSE.txt.
#if defined(VERTEX)
attribute vec4 VertexCoord;
attribute vec2 TexCoord;
uniform mat4 MVPMatrix;
varying vec2 uv;
void main() { gl_Position = MVPMatrix * VertexCoord; uv = TexCoord; }
#elif defined(FRAGMENT)
uniform sampler2D Texture;
uniform vec2 InputSize;
uniform vec2 OrigInputSize;
uniform vec2 OutputSize;
varying vec2 uv;
#pragma parameter KO_SCANLINE_STRENGTH "Scanline strength" 0.20 0.0 0.70 0.01
#ifdef PARAMETER_UNIFORM
uniform float KO_SCANLINE_STRENGTH;
#else
#define KO_SCANLINE_STRENGTH 0.20
#endif
void main() {
    vec4 color = texture2D(Texture, uv);
    float line = 0.5 - 0.5 * cos(uv.y * OrigInputSize.y * 6.28318530718);
    color.rgb *= 1.0 - KO_SCANLINE_STRENGTH * line;
    gl_FragColor = color;
}
#endif
