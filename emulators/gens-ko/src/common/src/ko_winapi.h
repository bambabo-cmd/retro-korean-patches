/* Gens KO, 2026-11-20: UTF-8 / Unicode bridge. GPLv2; see LICENSE.txt. */
#ifndef GENS_KO_WINAPI_H
#define GENS_KO_WINAPI_H
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#ifdef __cplusplus
#include <string>
std::wstring KoWide(const char *text);
std::string KoUtf8(const wchar_t *text);
extern "C" {
#endif
const char *KoText(const char *key, const char *fallback);
int KoMessageBox(HWND, const char *, const char *, UINT);
BOOL KoSetWindowText(HWND, const char *);
BOOL KoSetDlgItemText(HWND, int, const char *);
UINT KoGetDlgItemText(HWND, int, char *, int);
int KoGetWindowText(HWND, char *, int);
LRESULT KoGetListText(HWND, int, int, char *, int);
BOOL KoInsertMenu(HMENU, UINT, UINT, UINT_PTR, const char *);
HANDLE KoCreateFile(const char *, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
FILE *KoFopen(const char *, const char *);
BOOL KoDeleteFile(const char *);
BOOL KoMoveFileEx(const char *, const char *, DWORD);
DWORD KoGetFileAttributes(const char *);
DWORD KoGetCurrentDirectory(DWORD, char *);
BOOL KoSetCurrentDirectory(const char *);
BOOL KoGetOpenFileName(LPOPENFILENAMEA);
BOOL KoGetSaveFileName(LPOPENFILENAMEA);
DWORD KoGetPrivateProfileString(const char *, const char *, const char *, char *, DWORD, const char *);
UINT KoGetPrivateProfileInt(const char *, const char *, INT, const char *);
BOOL KoWritePrivateProfileString(const char *, const char *, const char *, const char *);
HMODULE KoLoadLibrary(const char *);
void KoInitializePaths(void);
void KoTraceStart(void);
void KoTrace(const char *);
#ifdef __cplusplus
}
#endif
#ifndef GENS_KO_API_IMPL
#undef MessageBox
#define MessageBox KoMessageBox
#undef SetWindowText
#define SetWindowText KoSetWindowText
#undef GetWindowText
#define GetWindowText KoGetWindowText
#undef SetDlgItemText
#define SetDlgItemText KoSetDlgItemText
#undef GetDlgItemText
#define GetDlgItemText KoGetDlgItemText
#undef InsertMenu
#define InsertMenu KoInsertMenu
#undef CreateFile
#define CreateFile KoCreateFile
#define fopen KoFopen
#undef DeleteFile
#define DeleteFile KoDeleteFile
#undef MoveFileEx
#define MoveFileEx KoMoveFileEx
#undef GetFileAttributes
#define GetFileAttributes KoGetFileAttributes
#undef GetCurrentDirectory
#define GetCurrentDirectory KoGetCurrentDirectory
#undef SetCurrentDirectory
#define SetCurrentDirectory KoSetCurrentDirectory
#undef GetOpenFileName
#define GetOpenFileName KoGetOpenFileName
#undef GetSaveFileName
#define GetSaveFileName KoGetSaveFileName
#undef GetPrivateProfileString
#define GetPrivateProfileString KoGetPrivateProfileString
#undef GetPrivateProfileInt
#define GetPrivateProfileInt KoGetPrivateProfileInt
#undef WritePrivateProfileString
#define WritePrivateProfileString KoWritePrivateProfileString
#undef LoadLibrary
#define LoadLibrary KoLoadLibrary
#endif
#endif
