
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <wtsapi32.h>

#include "types.h"
#include "resource.h"
#include "logic.h"
#include "hud.h"
#include "tray.h"
#include "config_ui.h"
#include "utils.h"

Cfg   g_cfg     = { DEF_WORK_MIN, DEF_REST_MIN, FALSE };
State g_state   = ST_WORK;
State g_saved_state = ST_WORK;
int   g_elapsed = 0;
int   g_focus_elapsed = 0;
int   g_snoozed = 0;
BOOL  g_warned  = FALSE;

HWND  g_hMain = NULL;
HWND  g_hHUD  = NULL;
HWND  g_hCfg  = NULL;
HFONT g_hFont = NULL;
HFONT g_hFontHeavy = NULL;
HICON g_hIco  = NULL;

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_hMain = hwnd;
        g_hIco  = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APP));
        if (!g_hIco) {
            g_hIco = LoadIconW(NULL, IDI_APPLICATION);
        }
        g_hFont = Util_Font(11, FALSE);
        g_hFontHeavy = Util_HeavyFont(16);
        Config_Load();
        Tray_Init(hwnd);
        HUD_Create();
        SetTimer(hwnd, ID_TICK, 1000, NULL);
        WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
        return 0;

    case WM_TIMER:
        Logic_Tick();
        return 0;

    case WM_WTSSESSION_CHANGE:
        if (wp == WTS_SESSION_UNLOCK) {
            if (g_saved_state == ST_FOCUS) {
                g_state = ST_FOCUS;
            } else {
                Logic_StartWork();
            }
            HUD_Refresh();
            Tray_Update();
        } else if (wp == WTS_SESSION_LOCK) {
            if (g_state != ST_PAUSED) {
                g_saved_state = g_state;
                g_state = ST_PAUSED;
            } else {
                g_saved_state = ST_WORK;
            }
            HUD_Refresh();
            Tray_Update();
        }
        return 0;

    case WM_TRAY:
        switch (LOWORD(lp)) {
        case WM_RBUTTONUP:     Tray_Menu(hwnd); break;
        case WM_LBUTTONDBLCLK: Cfg_Open();      break;
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDM_SNOOZE:   Logic_Snooze(); break;
        case IDM_FOCUS:    Logic_StartFocus(); break;
        case IDM_STOP_FOCUS: Logic_StopFocus(); break;
        case IDM_SETTINGS: Cfg_Open();     break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        {
           NOTIFYICONDATA nid = {0};
           nid.cbSize = sizeof(nid);
           nid.hWnd = hwnd;
           nid.uID = 1;
           Shell_NotifyIconW(NIM_DELETE, &nid);
        }
        WTSUnRegisterSessionNotification(hwnd);
        KillTimer(hwnd, ID_TICK);
        if (g_hIco)  DestroyIcon(g_hIco);
        if (g_hFont) DeleteObject(g_hFont);
        if (g_hFontHeavy) DeleteObject(g_hFontHeavy);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE p, LPSTR cmd, int n) {
    (void)p; (void)cmd; (void)n;
    
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"EyeReminder_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0; // Exit if already running
    }

    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_UPDOWN_CLASS};
    InitCommonControlsEx(&ic);

    /* Register HUD class */
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = HUDProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = WC_HUD;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    /* Register Main message window class */
    wc.lpfnWndProc   = MainProc;
    wc.hbrBackground = NULL;
    wc.lpszClassName = WC_MAIN;
    RegisterClassExW(&wc);

    /* Create invisible main message window */
    CreateWindowExW(0, WC_MAIN, APP_NAME, 0,
        0,0,0,0, HWND_MESSAGE, NULL, hInst, NULL);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    
    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    return (int)m.wParam;
}
