
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

Cfg   g_cfg     = { DEF_WORK_MIN, DEF_FOCUS_MIN, FALSE, FALSE };
State g_state   = ST_WORK;
State g_saved_state = ST_WORK;
int   g_elapsed = 0;
volatile LONG g_focus_elapsed = 0;
volatile BOOL g_is_idle = FALSE;
int   g_snoozed = 0;
BOOL  g_warned  = FALSE;

HWND  g_hMain = NULL;
HWND  g_hHUD  = NULL;
HWND  g_hCfg  = NULL;
HFONT g_hFontHeavy = NULL;
HICON g_hIco  = NULL;

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_hMain = hwnd;
        Util_Log(L"Main window created");
        g_hIco  = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDI_APP));
        if (!g_hIco) {
            g_hIco = LoadIconW(NULL, IDI_APPLICATION);
        }
        g_hFontHeavy = Util_HeavyFont(14);
        Config_Load();
        Tray_Init(hwnd);
        HUD_Create();
        if (!SetTimer(hwnd, ID_TICK, 1000, NULL)) {
            Util_LogLastError(L"SetTimer(ID_TICK)");
            DestroyWindow(hwnd);
            return -1;
        }
        if (!WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION)) {
            Util_LogLastError(L"WTSRegisterSessionNotification");
        }
        return 0;

    case WM_TIMER:
        if (wp == ID_TICK) {
            Logic_Tick();
        }
        return 0;

    case WM_WTSSESSION_CHANGE:
        if (wp == WTS_SESSION_UNLOCK) {
            Logic_OnSessionUnlock();
        } else if (wp == WTS_SESSION_LOCK) {
            Logic_OnSessionLock();
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
        case IDM_RESET:    Logic_Reset(); break;
        case IDM_FOCUS:    Logic_StartFocus(); break;
        case IDM_STOP_FOCUS: Logic_StopFocus(); break;
        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;
        }
        return 0;

    case WM_DESTROY:
        {
           Util_Log(L"Main window destroy requested");
           NOTIFYICONDATA nid = {0};
           nid.cbSize = sizeof(nid);
           nid.hWnd = hwnd;
           nid.uID = 1;
           if (!Shell_NotifyIconW(NIM_DELETE, &nid)) {
               Util_Log(L"Shell_NotifyIconW(NIM_DELETE) failed");
           }
        }
        if (!WTSUnRegisterSessionNotification(hwnd)) {
            Util_LogLastError(L"WTSUnRegisterSessionNotification");
        }
        KillTimer(hwnd, ID_TICK);
        if (g_hCfg && IsWindow(g_hCfg)) {
            DestroyWindow(g_hCfg);
        }
        if (g_hHUD && IsWindow(g_hHUD)) {
            DestroyWindow(g_hHUD);
        }
        if (g_hIco)  DestroyIcon(g_hIco);
        if (g_hFontHeavy) DeleteObject(g_hFontHeavy);
        g_hMain = NULL;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE p, LPSTR cmd, int n) {
    (void)p; (void)cmd; (void)n;
    int exitCode = 1;
    int gm;

    Util_LogInit();
    Util_InstallCrashHandlers();

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"EyeReminder_SingleInstance_Mutex");
    if (!hMutex) {
        Util_LogLastError(L"CreateMutexW");
        Util_LogShutdown();
        return exitCode;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Util_Log(L"Another instance is already running; exiting");
        CloseHandle(hMutex);
        Util_LogShutdown();
        return 0; // Exit if already running
    }

    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_UPDOWN_CLASS};
    if (!InitCommonControlsEx(&ic)) {
        Util_LogLastError(L"InitCommonControlsEx");
    }

    /* Register HUD class */
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = HUDProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = WC_HUD;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Util_LogLastError(L"RegisterClassExW(WC_HUD)");
        goto cleanup;
    }

    /* Register Main message window class */
    wc.style         = 0;
    wc.lpfnWndProc   = MainProc;
    wc.hbrBackground = NULL;
    wc.lpszClassName = WC_MAIN;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Util_LogLastError(L"RegisterClassExW(WC_MAIN)");
        goto cleanup;
    }

    /* Create invisible main message window */
    HWND hMsgWnd = CreateWindowExW(0, WC_MAIN, APP_NAME, 0,
        0,0,0,0, HWND_MESSAGE, NULL, hInst, NULL);
    if (!hMsgWnd) {
        Util_LogLastError(L"CreateWindowExW(WC_MAIN)");
        goto cleanup;
    }
    Util_Log(L"Message window created successfully");

    MSG m;
    while ((gm = GetMessageW(&m, NULL, 0, 0)) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (gm == -1) {
        Util_LogLastError(L"GetMessageW");
    } else {
        exitCode = (int)m.wParam;
    }

cleanup:
    Util_LogFormat(L"WinMain cleanup: exitCode=%d", exitCode);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    Util_LogShutdown();
    return exitCode;
}
