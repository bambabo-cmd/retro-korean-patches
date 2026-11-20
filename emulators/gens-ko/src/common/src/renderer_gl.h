/* Gens KO renderer, 2026-11-20. GPL-2.0; see LICENSE.txt. */
#ifndef GENS_KO_RENDERER_GL_H
#define GENS_KO_RENDERER_GL_H
#include <windows.h>

/* Display lifetime is independent of focus, menu visibility, and client size. */
int Renderer_Init(HWND window);
void Renderer_Shutdown(void);
void Renderer_Submit565(const unsigned short *pixels, int stride_pixels,
                        int width, int height, int rgb555);
void Renderer_Repaint(HWND window);
void Renderer_Resize(HWND window);
void Renderer_DisplayChanged(HWND window);
void Renderer_SetVSync(int enabled);
void Renderer_SetOverlayUtf8(const char *text, int duration_ms);
void Renderer_ShowSettings(HWND parent);
int Renderer_LoadPreset(const wchar_t *path, HWND error_parent);
const wchar_t *Renderer_Status(void);

/* Call between BeginPaint/EndPaint; no default background erasure is needed.
   WM_ERASEBKGND should return 1. WM_SIZE should call Renderer_Resize.
   WM_DISPLAYCHANGE should call Renderer_DisplayChanged.
   Merely gaining/losing focus must NOT destroy/recreate the renderer. */
#endif
