#include "hud.h"
#include "types.h"
#include <stdio.h>
#include <dwmapi.h>
#include "tray.h"
#include "config_ui.h"

#define HUD_W         108
#define HUD_H         32
#define HUD_PAD_X     20
#define HUD_PAD_Y     80

static COLORREF s_hudBg      = RGB(40, 44, 40);   // Natural dark gray-green
static COLORREF s_hudProgW   = RGB(105, 145, 115); // Natural moss green
static COLORREF s_hudProgR   = RGB(100, 130, 160); // Natural slate blue
static COLORREF s_hudTextCol = RGB(250, 250, 250); // White text
static wchar_t  s_hudText[32];
static float    s_hudProgress = 0.0f;

void HUD_Refresh(void) {
    if (!g_hHUD) return;

    int remWork = g_cfg.workMin * 60 - g_elapsed;
    if (remWork < 0) remWork = 0;

    // Work: blue progress
    float total = g_cfg.workMin * 60.0f;
    s_hudProgress = (total > 0) ? (float)(total - remWork) / total : 1.0f;
    
    // If warning, text goes red, else stays white
    if (remWork <= WARN_SEC) {
        s_hudTextCol = RGB(215, 100, 90); // Muted Red
    } else {
        s_hudTextCol = RGB(250, 250, 250); // White
    }

    if (g_state == ST_PAUSED) {
        swprintf(s_hudText, 32, L"已暂停 (锁屏)");
    } else {
        swprintf(s_hudText, 32, L"%02d:%02d", remWork/60, remWork%60);
    }

    InvalidateRect(g_hHUD, NULL, FALSE);
    UpdateWindow(g_hHUD);
}

void HUD_Create(void) {
    POINT pt;
    GetCursorPos(&pt);
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);
    
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);

    int w = MulDiv(HUD_W, dpi, 96);
    int h = MulDiv(HUD_H, dpi, 96);
    int px = MulDiv(HUD_PAD_X, dpi, 96);
    int py = MulDiv(HUD_PAD_Y, dpi, 96);

    int x = mi.rcWork.right  - w - px;
    int y = mi.rcWork.bottom - h - py;

    g_hHUD = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        WC_HUD, NULL, WS_POPUP,
        x, y, w, h, NULL, NULL, GetModuleHandleW(NULL), NULL);

    // DWM: Apply native Windows 11 rounded corners & drop shadow
    int corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(g_hHUD, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    // Remove custom SetWindowRgn to allow DWM to work
    // Alpha 230/255 for a slight frosted glass look
    SetLayeredWindowAttributes(g_hHUD, 0, 240, LWA_ALPHA);
    ShowWindow(g_hHUD, SW_SHOWNOACTIVATE);
    HUD_Refresh();
}

LRESULT CALLBACK HUDProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        // 1. Fill base background
        HBRUSH hbrBg = CreateSolidBrush(s_hudBg);
        FillRect(hdc, &rc, hbrBg);
        DeleteObject(hbrBg);

        // 2. Draw Progress Bar background
        COLORREF pColor = s_hudProgW;
        HBRUSH hbrProg = CreateSolidBrush(pColor);
        RECT rcProg = rc;
        rcProg.right = rc.left + (int)((rc.right - rc.left) * s_hudProgress);
        FillRect(hdc, &rcProg, hbrProg);
        DeleteObject(hbrProg);

        // 3. Draw text manually to correct vertical optical alignment
        SelectObject(hdc, g_hFontHeavy);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, s_hudTextCol);
        
        SIZE sz;
        int len = wcslen(s_hudText);
        GetTextExtentPoint32W(hdc, s_hudText, len, &sz);
        int tx = rc.left + (rc.right - rc.left - sz.cx) / 2;
        
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        int nudge = MulDiv(2, dpi, 96);
        int ty = rc.top + (rc.bottom - rc.top - sz.cy) / 2 - nudge; // Nudge up for strict centering
        TextOutW(hdc, tx, ty, s_hudText, len);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCHITTEST: return HTCAPTION;
    case WM_NCRBUTTONUP:
        if (wp == HTCAPTION) {
            Tray_Menu(g_hMain);
        }
        return 0;
    case WM_NCLBUTTONDBLCLK:
        if (wp == HTCAPTION) {
            Cfg_Open();
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
