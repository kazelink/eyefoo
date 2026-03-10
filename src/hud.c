#include "hud.h"
#include "types.h"
#include "tray.h"
#include "config_ui.h"
#include "utils.h"
#include <stdio.h>
#include <wchar.h>
#include <dwmapi.h>

#define HUD_W         90
#define HUD_H         26
#define HUD_PAD_X     20
#define HUD_PAD_Y     80

static COLORREF s_hudBg = RGB(242, 242, 242);     // Light gray
static COLORREF s_hudProgW = RGB(178, 223, 138);  // User's #b2df8a (light green)
static COLORREF s_hudTextCol = RGB(51, 51, 51);   // User's #333
static wchar_t s_hudText[32];
static float s_hudProgress = 0.0f;

void HUD_Refresh(void) {
    int remWork;
    float total;

    if (!g_hHUD) {
        return;
    }

    remWork = g_cfg.workMin * 60 - g_elapsed;
    if (remWork < 0) {
        remWork = 0;
    }

    total = g_cfg.workMin * 60.0f;
    s_hudProgress = (total > 0) ? (float)remWork / total : 0.0f;
    s_hudTextCol = (remWork <= WARN_SEC) ? RGB(225, 30, 30) : RGB(51, 51, 51); // Darker red for warning to be readable on light background

    if (g_state == ST_PAUSED) {
        swprintf(s_hudText, 32, L"Paused");
    } else {
        swprintf(s_hudText, 32, L"%02d:%02d", remWork / 60, remWork % 60);
    }

    InvalidateRect(g_hHUD, NULL, FALSE);
    UpdateWindow(g_hHUD);
}

void HUD_UpdatePosition(void) {
    POINT pt;
    HMONITOR hMon;
    MONITORINFO mi = {0};
    HDC hdc;
    int dpi;
    int w;
    int h;
    int px;
    int py;
    int x;
    int y;

    if (!g_hHUD) return;

    GetCursorPos(&pt);
    hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) {
        return;
    }

    hdc = GetDC(NULL);
    dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);

    w = MulDiv(HUD_W, dpi, 96);
    h = MulDiv(HUD_H, dpi, 96);
    px = MulDiv(HUD_PAD_X, dpi, 96);
    py = MulDiv(HUD_PAD_Y, dpi, 96);
    x = mi.rcWork.right - w - px;
    y = mi.rcWork.bottom - h - py;

    SetWindowPos(g_hHUD, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

void HUD_Create(void) {
    POINT pt;
    HMONITOR hMon;
    MONITORINFO mi = {0};
    HDC hdc;
    int dpi;
    int w;
    int h;
    int corner;

    GetCursorPos(&pt);
    hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) {
        Util_LogLastError(L"GetMonitorInfoW");
        return;
    }

    hdc = GetDC(NULL);
    dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);

    w = MulDiv(HUD_W, dpi, 96);
    h = MulDiv(HUD_H, dpi, 96);

    g_hHUD = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        WC_HUD, NULL, WS_POPUP,
        0, 0, w, h, NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (!g_hHUD) {
        Util_LogLastError(L"CreateWindowExW(WC_HUD)");
        return;
    }

    corner = 3; /* DWMWCP_ROUNDSMALL */
    DwmSetWindowAttribute(g_hHUD, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    if (!SetLayeredWindowAttributes(g_hHUD, 0, 240, LWA_ALPHA)) {
        Util_LogLastError(L"SetLayeredWindowAttributes");
    }
    HUD_UpdatePosition();
    ShowWindow(g_hHUD, SW_SHOWNOACTIVATE);
    HUD_Refresh();
}

LRESULT CALLBACK HUDProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        COLORREF pColor;
        HBRUSH hbrBg;
        HBRUSH hbrProg;
        RECT rcProg;
        int dpi;
        TEXTMETRICW tm;
        int hudCenterY;
        int ty;

        GetClientRect(hwnd, &rc);

        hbrBg = CreateSolidBrush(s_hudBg);
        FillRect(hdc, &rc, hbrBg);
        DeleteObject(hbrBg);

        pColor = s_hudProgW;
        hbrProg = CreateSolidBrush(pColor);
        rcProg = rc;
        rcProg.right = rc.left + (int)((rc.right - rc.left) * s_hudProgress);
        FillRect(hdc, &rcProg, hbrProg);
        DeleteObject(hbrProg);

        HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFontHeavy);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, s_hudTextCol);

        dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        GetTextMetricsW(hdc, &tm);
        hudCenterY = rc.top + (rc.bottom - rc.top) / 2;

        {
            GLYPHMETRICS gm;
            MAT2 mat2;
            ZeroMemory(&mat2, sizeof(mat2));
            mat2.eM11.value = 1;
            mat2.eM22.value = 1;

            if (GetGlyphOutlineW(hdc, L'0', GGO_METRICS, &gm, 0, NULL, &mat2) != GDI_ERROR) {
                int baselineY = hudCenterY + (int)gm.gmptGlyphOrigin.y - (int)gm.gmBlackBoxY / 2;
                ty = baselineY - tm.tmAscent;
            } else {
                ty = hudCenterY - (tm.tmAscent + tm.tmInternalLeading) / 2;
            }
        }

        {
            wchar_t *colon = wcschr(s_hudText, L':');
            if (colon && wcslen(s_hudText) == 5) {
                int maxW = 0;
                wchar_t c;
                int dw;
                int colonW;
                int centerX;
                int tx[4];
                wchar_t chars[4];
                int i;
                HBRUSH hDot;
                HBRUSH oldB;
                HPEN hPen;
                HPEN oldP;
                int dotSize;
                int dotX1;
                int dotX2;
                int dotYOffset;
                int topDotY1;
                int topDotY2;
                int botDotY1;
                int botDotY2;

                for (c = L'0'; c <= L'9'; c++) {
                    SIZE sz;
                    GetTextExtentPoint32W(hdc, &c, 1, &sz);
                    if (sz.cx > maxW) {
                        maxW = sz.cx;
                    }
                }

                dw = maxW + MulDiv(2, dpi, 96);
                colonW = MulDiv(10, dpi, 96);
                centerX = rc.left + (rc.right - rc.left) / 2;

                tx[2] = centerX + colonW / 2;
                tx[3] = tx[2] + dw;
                tx[1] = centerX - colonW / 2 - dw;
                tx[0] = tx[1] - dw;

                chars[0] = s_hudText[0];
                chars[1] = s_hudText[1];
                chars[2] = s_hudText[3];
                chars[3] = s_hudText[4];
                for (i = 0; i < 4; i++) {
                    SIZE sz;
                    int offX;
                    GetTextExtentPoint32W(hdc, &chars[i], 1, &sz);
                    offX = (dw - sz.cx) / 2;
                    TextOutW(hdc, tx[i] + offX, ty, &chars[i], 1);
                }

                hDot = CreateSolidBrush(s_hudTextCol);
                oldB = SelectObject(hdc, hDot);
                hPen = CreatePen(PS_SOLID, 1, s_hudTextCol);
                oldP = SelectObject(hdc, hPen);

                dotSize = MulDiv(4, dpi, 96);
                if (dotSize < 3) {
                    dotSize = 3;
                }

                dotX1 = centerX - dotSize / 2;
                dotX2 = dotX1 + dotSize;
                dotYOffset = MulDiv(4, dpi, 96);
                topDotY1 = hudCenterY - dotYOffset - dotSize / 2;
                topDotY2 = topDotY1 + dotSize;
                botDotY1 = hudCenterY + dotYOffset - dotSize / 2;
                botDotY2 = botDotY1 + dotSize;

                Ellipse(hdc, dotX1, topDotY1, dotX2, topDotY2);
                Ellipse(hdc, dotX1, botDotY1, dotX2, botDotY2);

                SelectObject(hdc, oldB);
                SelectObject(hdc, oldP);
                DeleteObject(hDot);
                DeleteObject(hPen);
            } else {
                SIZE sz;
                int len = (int)wcslen(s_hudText);
                int tx;
                GetTextExtentPoint32W(hdc, s_hudText, len, &sz);
                tx = rc.left + (rc.right - rc.left - sz.cx) / 2;
                TextOutW(hdc, tx, ty, s_hudText, len);
            }
        }

        SelectObject(hdc, hOldFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_NCHITTEST:
        return HTCAPTION;
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
    case WM_DESTROY:
        if (hwnd == g_hHUD) {
            g_hHUD = NULL;
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}
