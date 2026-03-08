#include "utils.h"
#include "types.h"
#include <stdio.h>

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
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk)) return;
    if (on) {
        wchar_t p[MAX_PATH];
        GetModuleFileNameW(NULL, p, MAX_PATH);
        RegSetValueExW(hk, APP_NAME, 0, REG_SZ, (BYTE*)p, (wcslen(p)+1)*2);
    } else {
        RegDeleteValueW(hk, APP_NAME);
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
    g_cfg.autoStart = GetPrivateProfileIntW(L"Settings", L"AutoStart", 0,              path);
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
