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
void main() { gl_FragColor = texture2D(Texture, uv); }
#endif
