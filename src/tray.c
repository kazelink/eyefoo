#include "tray.h"
#include "types.h"
#include "resource.h"
#include "utils.h"
#include <shellapi.h>
#include <stdio.h>

static NOTIFYICONDATA g_nid;

static void Tray_Notify(UINT message, const wchar_t *context) {
    if (!Shell_NotifyIconW(message, &g_nid)) {
        Util_Log(context);
    }
}

void Tray_Init(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon            = g_hIco;
    wcscpy(g_nid.szTip, APP_NAME);
    Tray_Notify(NIM_ADD, L"Shell_NotifyIconW(NIM_ADD) failed");
}

void Tray_Update(void) {
    if (g_state == ST_PAUSED) {
        wcscpy(g_nid.szTip, L"已暂停 (锁屏)");
    } else {
        int rem = g_cfg.workMin * 60 - g_elapsed;
        if (rem < 0) rem = 0;
        swprintf(g_nid.szTip, 128, L"%d:%02d  后锁屏", rem/60, rem%60);
    }
    g_nid.uFlags = NIF_TIP;
    Tray_Notify(NIM_MODIFY, L"Shell_NotifyIconW(NIM_MODIFY tip) failed");
}

void Tray_Balloon(const wchar_t *title, const wchar_t *msg) {
    g_nid.uFlags      = NIF_INFO;
    g_nid.uTimeout    = 5000;
    g_nid.dwInfoFlags = NIIF_NOSOUND;
    wcscpy(g_nid.szInfoTitle, title);
    wcscpy(g_nid.szInfo, msg);
    Tray_Notify(NIM_MODIFY, L"Shell_NotifyIconW(NIM_MODIFY balloon) failed");
}

void Tray_Menu(HWND hwnd) {
    POINT pt; GetCursorPos(&pt);
    HMENU hm = CreatePopupMenu();
    if (!hm) {
        Util_LogLastError(L"CreatePopupMenu");
        return;
    }
    MENUINFO mi = {0};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_STYLE;
    mi.dwStyle = MNS_NOCHECK;
    if (!SetMenuInfo(hm, &mi)) {
        Util_LogLastError(L"SetMenuInfo");
    }

    if (g_state == ST_WORK) {
        if (g_snoozed < MAX_SNOOZE) {
            AppendMenuW(hm, MF_STRING, IDM_SNOOZE, L"推迟 5 分钟");
        }
        AppendMenuW(hm, MF_STRING, IDM_RESET, L"重置倒计时");
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    }
    if (g_state != ST_FOCUS) {
        wchar_t focusText[64];
        if (g_cfg.focusMin >= 60 && g_cfg.focusMin % 60 == 0) {
            swprintf(focusText, 64, L"专注模式 (%d 小时)", g_cfg.focusMin / 60);
        } else {
            swprintf(focusText, 64, L"专注模式 (%d 分钟)", g_cfg.focusMin);
        }
        AppendMenuW(hm, MF_STRING, IDM_FOCUS, focusText);
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    } else {
        AppendMenuW(hm, MF_STRING, IDM_STOP_FOCUS, L"结束专注");
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    }

    AppendMenuW(hm, MF_STRING, IDM_EXIT, L"退出");

    SetForegroundWindow(hwnd);
    if (!TrackPopupMenuEx(hm, TPM_RIGHTALIGN|TPM_BOTTOMALIGN|TPM_RIGHTBUTTON,
        pt.x, pt.y, hwnd, NULL)) {
        Util_LogLastError(L"TrackPopupMenuEx");
    }
    DestroyMenu(hm);
}
