#include "tray.h"
#include "types.h"
#include "resource.h"
#include "utils.h"
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

static NOTIFYICONDATA g_nid;

static void Tray_Notify(UINT message, const wchar_t *context) {
    if (!Shell_NotifyIconW(message, &g_nid)) {
        Util_Log(context);
    }
}

void Tray_Init(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = g_hIco;
    wcscpy(g_nid.szTip, APP_NAME);
    Tray_Notify(NIM_ADD, L"Shell_NotifyIconW(NIM_ADD) failed");
}

void Tray_Update(void) {
    if (g_state == ST_PAUSED) {
        wcscpy(g_nid.szTip, L"Paused (locked)");
    } else {
        int rem = g_cfg.workMin * 60 - g_elapsed;
        if (rem < 0) {
            rem = 0;
        }
        swprintf(g_nid.szTip, 128, L"%d:%02d until lock", rem / 60, rem % 60);
    }
    g_nid.uFlags = NIF_TIP;
    Tray_Notify(NIM_MODIFY, L"Shell_NotifyIconW(NIM_MODIFY tip) failed");
}

void Tray_Balloon(const wchar_t *title, const wchar_t *msg) {
    g_nid.uFlags = NIF_INFO;
    g_nid.uTimeout = 5000;
    g_nid.dwInfoFlags = NIIF_NOSOUND;
    wcscpy(g_nid.szInfoTitle, title);
    wcscpy(g_nid.szInfo, msg);
    Tray_Notify(NIM_MODIFY, L"Shell_NotifyIconW(NIM_MODIFY balloon) failed");
}

void Tray_Menu(HWND hwnd) {
    POINT pt;
    HMENU hm;
    MENUINFO mi = {0};

    GetCursorPos(&pt);
    hm = CreatePopupMenu();
    if (!hm) {
        Util_LogLastError(L"CreatePopupMenu");
        return;
    }

    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE;
    mi.dwStyle = MNS_NOCHECK;
    if (!SetMenuInfo(hm, &mi)) {
        Util_LogLastError(L"SetMenuInfo");
    }

    if (g_state == ST_WORK) {
        if (g_snoozed < MAX_SNOOZE) {
            AppendMenuW(hm, MF_STRING, IDM_SNOOZE, L"Snooze 5 min");
        }
        AppendMenuW(hm, MF_STRING, IDM_RESET, L"Reset timer");
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    }

    if (g_state != ST_FOCUS) {
        wchar_t focusText[64];
        if (g_cfg.focusMin >= 60 && g_cfg.focusMin % 60 == 0) {
            swprintf(focusText, 64, L"Focus mode (%d hour)", g_cfg.focusMin / 60);
        } else {
            swprintf(focusText, 64, L"Focus mode (%d min)", g_cfg.focusMin);
        }
        AppendMenuW(hm, MF_STRING, IDM_FOCUS, focusText);
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    } else {
        AppendMenuW(hm, MF_STRING, IDM_STOP_FOCUS, L"Stop focus mode");
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    }

    AppendMenuW(hm, MF_STRING, IDM_EXIT, L"Exit");

    SetForegroundWindow(hwnd);
    SetLastError(ERROR_SUCCESS);
    if (!TrackPopupMenuEx(hm, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
        pt.x, pt.y, hwnd, NULL) && GetLastError() != ERROR_SUCCESS) {
        Util_LogLastError(L"TrackPopupMenuEx");
    }
    DestroyMenu(hm);
}
