#include "utils.h"
#include "types.h"
#include <stdio.h>

void Util_Log(const wchar_t *msg) {
    if (!msg) return;
    OutputDebugStringW(L"[EyeReminder] ");
    OutputDebugStringW(msg);
    OutputDebugStringW(L"\n");
}

void Util_LogLastError(const wchar_t *api) {
    wchar_t buf[256];
    DWORD err = GetLastError();
    swprintf(buf, 256, L"%ls failed (error=%lu)", api ? api : L"(unknown)", (unsigned long)err);
    Util_Log(buf);
}

BOOL Reg_GetAutoStart(void) {
    HKEY hk; BOOL on = FALSE;
    if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hk)) {
        on = !RegQueryValueExW(hk, APP_NAME, NULL, NULL, NULL, NULL);
        RegCloseKey(hk);
    }
    return on;
}

void Reg_SetAutoStart(BOOL on) {
    HKEY hk;
    LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk);
    if (rc != ERROR_SUCCESS) {
        SetLastError((DWORD)rc);
        Util_LogLastError(L"RegOpenKeyExW(Run)");
        return;
    }

    if (on) {
        wchar_t p[MAX_PATH];
        if (!GetModuleFileNameW(NULL, p, MAX_PATH)) {
            Util_LogLastError(L"GetModuleFileNameW");
            RegCloseKey(hk);
            return;
        }

        wchar_t cmd[MAX_PATH + 3];
        swprintf(cmd, MAX_PATH + 3, L"\"%ls\"", p);
        rc = RegSetValueExW(hk, APP_NAME, 0, REG_SZ, (BYTE*)cmd,
            (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
        if (rc != ERROR_SUCCESS) {
            SetLastError((DWORD)rc);
            Util_LogLastError(L"RegSetValueExW(AutoStart)");
        }
    } else {
        rc = RegDeleteValueW(hk, APP_NAME);
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) {
            SetLastError((DWORD)rc);
            Util_LogLastError(L"RegDeleteValueW(AutoStart)");
        }
    }
    RegCloseKey(hk);
}

static void GetIniPath(wchar_t* path) {
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) {
        wcscpy(lastSlash + 1, L"config.ini");
    }
}

void Config_Load(void) {
    wchar_t path[MAX_PATH];
    GetIniPath(path);
    g_cfg.workMin   = GetPrivateProfileIntW(L"Settings", L"WorkMin",   DEF_WORK_MIN,   path);
    g_cfg.focusMin  = GetPrivateProfileIntW(L"Settings", L"FocusMin",  DEF_FOCUS_MIN,  path);
    if (g_cfg.workMin < 1 || g_cfg.workMin > 180) g_cfg.workMin = DEF_WORK_MIN;
    if (g_cfg.focusMin < 10 || g_cfg.focusMin > 720) g_cfg.focusMin = DEF_FOCUS_MIN;
    g_cfg.autoStart = Reg_GetAutoStart();
}

void Config_Save(void) {
    wchar_t path[MAX_PATH];
    GetIniPath(path);
    wchar_t bWork[16], bFocus[16], bAuto[16];
    swprintf(bWork, 16, L"%d", g_cfg.workMin);
    swprintf(bFocus, 16, L"%d", g_cfg.focusMin);
    swprintf(bAuto, 16, L"%d", g_cfg.autoStart ? 1 : 0);

    WritePrivateProfileStringW(L"Settings", L"WorkMin",   bWork, path);
    WritePrivateProfileStringW(L"Settings", L"FocusMin",  bFocus, path);
    WritePrivateProfileStringW(L"Settings", L"AutoStart", bAuto, path);
}

HFONT Util_Font(int pt, BOOL bold) {
    HDC hdc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);
    return CreateFontW(h, 0,0,0, bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE,FALSE,FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI"); // Change to Microsoft YaHei UI for better Chinese readability
}

HFONT Util_HeavyFont(int pt) {
    HDC hdc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);
    return CreateFontW(h, 0,0,0, FW_BOLD,
        FALSE,FALSE,FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}
