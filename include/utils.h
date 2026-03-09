#ifndef UTILS_H
#define UTILS_H

#include <windows.h>

BOOL Reg_GetAutoStart(void);
void Reg_SetAutoStart(BOOL on);
void Config_Load(void);
void Config_Save(void);
HFONT Util_Font(int pt, BOOL bold);
HFONT Util_HeavyFont(int pt);
void Util_LogInit(void);
void Util_LogShutdown(void);
void Util_Log(const wchar_t *msg);
void Util_LogFormat(const wchar_t *fmt, ...);
void Util_LogLastError(const wchar_t *api);
void Util_InstallCrashHandlers(void);

#endif
