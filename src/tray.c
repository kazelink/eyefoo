#include "tray.h"
#include "types.h"
#include "resource.h"
#include <shellapi.h>
#include <stdio.h>

static NOTIFYICONDATA g_nid;

void Tray_Init(HWND hwnd) {
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hwnd;
    g_nid.uID              = 1;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon            = g_hIco;
    wcscpy(g_nid.szTip, APP_NAME);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
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
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void Tray_Balloon(const wchar_t *title, const wchar_t *msg) {
    g_nid.uFlags      = NIF_INFO;
    g_nid.uTimeout    = 5000;
    g_nid.dwInfoFlags = NIIF_NOSOUND;
    wcscpy(g_nid.szInfoTitle, title);
    wcscpy(g_nid.szInfo, msg);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void Tray_Menu(HWND hwnd) {
    POINT pt; GetCursorPos(&pt);
    HMENU hm = CreatePopupMenu();

    if (g_state == ST_WORK && g_snoozed < MAX_SNOOZE) {
        AppendMenuW(hm, MF_STRING, IDM_SNOOZE, L"推迟 5 分钟");
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    }
    if (g_state != ST_FOCUS) {
        AppendMenuW(hm, MF_STRING, IDM_FOCUS, L"专注模式 (2小时)");
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    } else {
        AppendMenuW(hm, MF_STRING, IDM_STOP_FOCUS, L"结束专注");
        AppendMenuW(hm, MF_SEPARATOR, 0, NULL);
    }

    AppendMenuW(hm, MF_STRING, IDM_EXIT, L"退出");

    SetForegroundWindow(hwnd);
    TrackPopupMenuEx(hm, TPM_RIGHTALIGN|TPM_BOTTOMALIGN|TPM_RIGHTBUTTON,
        pt.x, pt.y, hwnd, NULL);
    DestroyMenu(hm);
}
