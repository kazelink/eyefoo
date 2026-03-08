#include "hud.h"
#include "types.h"
#include <stdio.h>
#include <dwmapi.h>
#include "tray.h"
#include "config_ui.h"
#include "utils.h"

#define HUD_W         90
#define HUD_H         26
#define HUD_PAD_X     20
#define HUD_PAD_Y     80

static COLORREF s_hudBg      = RGB(40, 44, 40);   // Natural dark gray-green
static COLORREF s_hudProgW   = RGB(105, 145, 115); // Natural moss green
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
        s_hudTextCol = RGB(179, 0, 0); // #b30000
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
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) {
        Util_LogLastError(L"GetMonitorInfoW");
        return;
    }
    
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
    if (!g_hHUD) {
        Util_LogLastError(L"CreateWindowExW(WC_HUD)");
        return;
    }

    // DWM: Apply native Windows 11 rounded corners & drop shadow
    int corner = DWMWCP_ROUND;
    HRESULT hr = DwmSetWindowAttribute(g_hHUD, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    if (FAILED(hr)) {
        wchar_t msg[128];
        swprintf(msg, 128, L"DwmSetWindowAttribute failed (hr=0x%08X)", (unsigned int)hr);
        Util_Log(msg);
    }

    // Remove custom SetWindowRgn to allow DWM to work
    // Alpha 230/255 for a slight frosted glass look
    if (!SetLayeredWindowAttributes(g_hHUD, 0, 240, LWA_ALPHA)) {
        Util_LogLastError(L"SetLayeredWindowAttributes");
    }
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
        
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);

        TEXTMETRICW tm;
        GetTextMetricsW(hdc, &tm);
        
        int hudCenterY = rc.top + (rc.bottom - rc.top) / 2;
        int ty;

        GLYPHMETRICS gm;
        MAT2 mat2;
        ZeroMemory(&mat2, sizeof(mat2));
        mat2.eM11.value = 1; mat2.eM22.value = 1;
        
        if (GetGlyphOutlineW(hdc, L'0', GGO_METRICS, &gm, 0, NULL, &mat2) != GDI_ERROR) {
            // gm.gmptGlyphOrigin.y: from baseline UP to the top of the glyph ink.
            // gm.gmBlackBoxY: total height of the glyph ink.
            // Ink center distance from baseline = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY / 2
            // To place ink center exactly at hudCenterY:
            int baselineY = hudCenterY + (int)gm.gmptGlyphOrigin.y - (int)gm.gmBlackBoxY / 2;
            ty = baselineY - tm.tmAscent;
        } else {
            ty = hudCenterY - (tm.tmAscent + tm.tmInternalLeading) / 2;
        }

        wchar_t* colon = wcschr(s_hudText, L':');
        if (colon && wcslen(s_hudText) == 5) {
            int maxW = 0;
            for (wchar_t c = L'0'; c <= L'9'; c++) {
                SIZE sz; GetTextExtentPoint32W(hdc, &c, 1, &sz);
                if (sz.cx > maxW) maxW = sz.cx;
            }
            
            int dw = maxW + MulDiv(2, dpi, 96); // Reduced spacing
            int colonW = MulDiv(10, dpi, 96);   // Reduced colon width
            
            int centerX = rc.left + (rc.right - rc.left) / 2;
            
            int tx[4];
            tx[2] = centerX + colonW / 2;       
            tx[3] = tx[2] + dw;                 
            tx[1] = centerX - colonW / 2 - dw;  
            tx[0] = tx[1] - dw;                 
            
            wchar_t chars[4] = { s_hudText[0], s_hudText[1], s_hudText[3], s_hudText[4] };
            for(int i = 0; i < 4; i++) {
                SIZE sz; GetTextExtentPoint32W(hdc, &chars[i], 1, &sz);
                int offX = (dw - sz.cx) / 2;
                TextOutW(hdc, tx[i] + offX, ty, &chars[i], 1);
            }
            
            HBRUSH hDot = CreateSolidBrush(s_hudTextCol);
            HBRUSH oldB = SelectObject(hdc, hDot);
            HPEN hPen = CreatePen(PS_SOLID, 1, s_hudTextCol);
            HPEN oldP = SelectObject(hdc, hPen);
            
            int dotSize = MulDiv(4, dpi, 96);
            if (dotSize < 3) dotSize = 3;
            
            int dotX1 = centerX - dotSize / 2;
            int dotX2 = dotX1 + dotSize;
            
            int dotYOffset = MulDiv(4, dpi, 96);
            
            // Colons are mathematically centered against hudCenterY (same pivot as the digits' ink)
            int topDotY1 = hudCenterY - dotYOffset - dotSize / 2;
            int topDotY2 = topDotY1 + dotSize;
            
            int botDotY1 = hudCenterY + dotYOffset - dotSize / 2;
            int botDotY2 = botDotY1 + dotSize;
            
            Ellipse(hdc, dotX1, topDotY1, dotX2, topDotY2);
            Ellipse(hdc, dotX1, botDotY1, dotX2, botDotY2);
            
            SelectObject(hdc, oldB);
            SelectObject(hdc, oldP);
            DeleteObject(hDot);
            DeleteObject(hPen);
        } else {
            SIZE sz;
            int len = wcslen(s_hudText);
            GetTextExtentPoint32W(hdc, s_hudText, len, &sz);
            int tx = rc.left + (rc.right - rc.left - sz.cx) / 2;
            TextOutW(hdc, tx, ty, s_hudText, len);
        }

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
