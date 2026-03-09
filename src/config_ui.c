#include "config_ui.h"
#include "types.h"
#include "resource.h"
#include "utils.h"
#include "logic.h"
#include "hud.h"
#include "tray.h"
#include <commctrl.h>
#include <stdio.h>

static HWND Ctrl(const wchar_t *cls, const wchar_t *txt, DWORD style,
                 int x, int y, int w, int h, HWND parent, int id, HFONT font) {
    HWND hw = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (!hw) {
        Util_LogLastError(L"CreateWindowExW(Ctrl)");
    }
    if (font) {
        SendMessageW(hw, WM_SETFONT, (WPARAM)font, TRUE);
    }
    return hw;
}

static HWND EditBox(const wchar_t *val, int x, int y, int w, int h,
                    HWND parent, int id, HFONT font) {
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", val,
        WS_CHILD | WS_VISIBLE | ES_NUMBER | ES_CENTER,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (!hw) {
        Util_LogLastError(L"CreateWindowExW(EditBox)");
    }
    if (font) {
        SendMessageW(hw, WM_SETFONT, (WPARAM)font, TRUE);
    }
    return hw;
}

static void Spin(HWND buddy, int lo, int hi, int val, HWND parent, int id) {
    HWND hw = CreateWindowExW(0, UPDOWN_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (!hw) {
        Util_LogLastError(L"CreateWindowExW(Spin)");
        return;
    }
    SendMessageW(hw, UDM_SETBUDDY, (WPARAM)buddy, 0);
    SendMessageW(hw, UDM_SETRANGE32, lo, hi);
    SendMessageW(hw, UDM_SETPOS32, 0, val);
}

static int ScaleDpi(int value, UINT dpi) {
    return MulDiv(value, (int)dpi, 96);
}

static UINT DpiOfWindowOrScreen(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    UINT dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hwnd, hdc);
    return dpi ? dpi : 96;
}

void Cfg_Open(void) {
    UINT dpi;
    RECT rc;
    RECT wa;
    int width;
    int height;
    int x;
    int y;
    WNDCLASSEXW wc = {0};

    if (g_hCfg && IsWindow(g_hCfg)) {
        SetForegroundWindow(g_hCfg);
        return;
    }

    dpi = DpiOfWindowOrScreen(NULL);
    rc.left = 0;
    rc.top = 0;
    rc.right = ScaleDpi(250, dpi);
    rc.bottom = ScaleDpi(190, dpi);
    AdjustWindowRectEx(&rc,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        FALSE,
        WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW);

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    width = rc.right - rc.left;
    height = rc.bottom - rc.top;
    x = (wa.right + wa.left - width) / 2;
    y = (wa.bottom + wa.top - height) / 2;

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = CfgProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = WC_CFG;
    wc.hIcon = g_hIco;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        Util_LogLastError(L"RegisterClassExW(WC_CFG)");
        return;
    }

    g_hCfg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_APPWINDOW,
        WC_CFG, APP_NAME L" - Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, width, height, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_hCfg) {
        Util_LogLastError(L"CreateWindowExW(WC_CFG)");
        return;
    }

    ShowWindow(g_hCfg, SW_SHOW);
    UpdateWindow(g_hCfg);
}

LRESULT CALLBACK CfgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HFONT fTitle = NULL;
    static HFONT fNormal = NULL;
    static HFONT fSmall = NULL;

    (void)lp;

    switch (msg) {
    case WM_CREATE: {
        UINT dpi = DpiOfWindowOrScreen(hwnd);
        wchar_t workBuf[8];
        wchar_t focusBuf[8];
        HWND workEdit;
        HWND focusEdit;
        HWND autoStart;
        HWND debugLog;

        fTitle = Util_Font(11, TRUE);
        fNormal = Util_Font(10, FALSE);
        fSmall = Util_Font(9, FALSE);

        Ctrl(L"STATIC", L"Work minutes", SS_LEFT | SS_CENTERIMAGE,
            ScaleDpi(20, dpi), ScaleDpi(20, dpi),
            ScaleDpi(100, dpi), ScaleDpi(24, dpi), hwnd, 0, fNormal);

        swprintf(workBuf, 8, L"%d", g_cfg.workMin);
        workEdit = EditBox(workBuf, ScaleDpi(130, dpi), ScaleDpi(20, dpi),
            ScaleDpi(60, dpi), ScaleDpi(24, dpi), hwnd, IDC_WORK_E, fNormal);
        Spin(workEdit, 1, 180, g_cfg.workMin, hwnd, IDC_WORK_S);

        Ctrl(L"STATIC", L"Focus minutes", SS_LEFT | SS_CENTERIMAGE,
            ScaleDpi(20, dpi), ScaleDpi(55, dpi),
            ScaleDpi(100, dpi), ScaleDpi(24, dpi), hwnd, 0, fNormal);

        swprintf(focusBuf, 8, L"%d", g_cfg.focusMin);
        focusEdit = EditBox(focusBuf, ScaleDpi(130, dpi), ScaleDpi(55, dpi),
            ScaleDpi(60, dpi), ScaleDpi(24, dpi), hwnd, IDC_FOCUS_E, fNormal);
        Spin(focusEdit, 10, 720, g_cfg.focusMin, hwnd, IDC_FOCUS_S);

        autoStart = Ctrl(L"BUTTON", L"Start with Windows", BS_AUTOCHECKBOX,
            ScaleDpi(20, dpi), ScaleDpi(90, dpi),
            ScaleDpi(180, dpi), ScaleDpi(24, dpi), hwnd, IDC_AUTO, fNormal);
        SendMessageW(autoStart, BM_SETCHECK, g_cfg.autoStart ? BST_CHECKED : BST_UNCHECKED, 0);

        debugLog = Ctrl(L"BUTTON", L"Debug log", BS_AUTOCHECKBOX,
            ScaleDpi(20, dpi), ScaleDpi(115, dpi),
            ScaleDpi(180, dpi), ScaleDpi(24, dpi), hwnd, IDC_DEBUG, fNormal);
        SendMessageW(debugLog, BM_SETCHECK, g_cfg.debugLog ? BST_CHECKED : BST_UNCHECKED, 0);

        Ctrl(L"BUTTON", L"Save", BS_DEFPUSHBUTTON,
            ScaleDpi(90, dpi), ScaleDpi(145, dpi),
            ScaleDpi(70, dpi), ScaleDpi(30, dpi), hwnd, IDC_OK, fTitle);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_OK: {
            wchar_t workBuf[8];
            wchar_t focusBuf[8];
            int workMin;
            int focusMin;

            GetWindowTextW(GetDlgItem(hwnd, IDC_WORK_E), workBuf, 8);
            GetWindowTextW(GetDlgItem(hwnd, IDC_FOCUS_E), focusBuf, 8);
            workMin = _wtoi(workBuf);
            focusMin = _wtoi(focusBuf);

            if (workMin < 1 || workMin > 180) {
                MessageBoxW(hwnd, L"Work minutes must be between 1 and 180.", APP_NAME, MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (focusMin < 10 || focusMin > 720) {
                MessageBoxW(hwnd, L"Focus minutes must be between 10 and 720.", APP_NAME, MB_OK | MB_ICONWARNING);
                return 0;
            }

            g_cfg.workMin = workMin;
            g_cfg.focusMin = focusMin;
            g_cfg.autoStart = (BST_CHECKED == SendMessageW(GetDlgItem(hwnd, IDC_AUTO), BM_GETCHECK, 0, 0));
            g_cfg.debugLog = (BST_CHECKED == SendMessageW(GetDlgItem(hwnd, IDC_DEBUG), BM_GETCHECK, 0, 0));
            Reg_SetAutoStart(g_cfg.autoStart);
            Config_Save();
            Logic_StartWork();
            HUD_Refresh();
            Tray_Update();
            DestroyWindow(hwnd);
            return 0;
        }
        }
        return 0;
    case WM_DESTROY:
        if (fTitle) {
            DeleteObject(fTitle);
            fTitle = NULL;
        }
        if (fNormal) {
            DeleteObject(fNormal);
            fNormal = NULL;
        }
        if (fSmall) {
            DeleteObject(fSmall);
            fSmall = NULL;
        }
        g_hCfg = NULL;
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}
