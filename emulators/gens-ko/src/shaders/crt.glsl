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
#pragma parameter KO_SCANLINE_STRENGTH "Scanline strength" 0.18 0.0 0.70 0.01
#pragma parameter KO_MASK_STRENGTH "RGB mask strength" 0.10 0.0 0.50 0.01
#pragma parameter KO_CURVATURE "Screen curvature" 0.025 0.0 0.15 0.005
#ifdef PARAMETER_UNIFORM
uniform float KO_SCANLINE_STRENGTH;
uniform float KO_MASK_STRENGTH;
uniform float KO_CURVATURE;
#else
#define KO_SCANLINE_STRENGTH 0.18
#define KO_MASK_STRENGTH 0.10
#define KO_CURVATURE 0.025
#endif
void main() {
    vec2 centered = uv * 2.0 - 1.0;
    vec2 bend = centered * (1.0 + KO_CURVATURE * dot(centered, centered));
    vec2 sampleUV = bend * 0.5 + 0.5;
    if (min(sampleUV.x, sampleUV.y) < 0.0 || max(sampleUV.x, sampleUV.y) > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0); return;
    }
    vec3 color = texture2D(Texture, sampleUV).rgb;
    float line = 0.5 - 0.5 * cos(sampleUV.y * OrigInputSize.y * 6.28318530718);
    float stripe = mod(floor(uv.x * OutputSize.x), 3.0);
    vec3 mask = vec3(1.0 - KO_MASK_STRENGTH);
    if (stripe < 1.0) mask.r = 1.0;
    else if (stripe < 2.0) mask.g = 1.0;
    else mask.b = 1.0;
    color *= (1.0 - KO_SCANLINE_STRENGTH * line) * mask;
    gl_FragColor = vec4(color, 1.0);
}
#endif
