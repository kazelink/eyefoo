#include "config_ui.h"
#include "types.h"
#include "resource.h"
#include "utils.h"
#include "logic.h"
#include "hud.h"
#include "tray.h"
#include <commctrl.h>
#include <stdio.h>

/* 内联辅助：创建子控件 */
static HWND _Ctrl(const wchar_t *cls, const wchar_t *txt, DWORD sty,
                  int x,int y,int w,int h, HWND p, int id, HFONT f) {
    HWND hw = CreateWindowExW(0, cls, txt, WS_CHILD|WS_VISIBLE|sty,
        x,y,w,h, p, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    if (f) SendMessageW(hw, WM_SETFONT, (WPARAM)f, TRUE);
    return hw;
}
static HWND _Edit(const wchar_t *val, int x,int y,int w,int h,
                  HWND p, int id, HFONT f) {
    HWND hw = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", val,
        WS_CHILD|WS_VISIBLE|ES_NUMBER|ES_CENTER, x,y,w,h,
        p,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    if (f) SendMessageW(hw, WM_SETFONT, (WPARAM)f, TRUE);
    return hw;
}
static void _Spin(HWND buddy, int lo,int hi,int val, HWND p, int id) {
    HWND hw = CreateWindowExW(0, UPDOWN_CLASSW, NULL,
        WS_CHILD|WS_VISIBLE|UDS_SETBUDDYINT|UDS_ALIGNRIGHT|UDS_ARROWKEYS,
        0,0,0,0, p,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(hw, UDM_SETBUDDY,   (WPARAM)buddy, 0);
    SendMessageW(hw, UDM_SETRANGE32, lo, hi);
    SendMessageW(hw, UDM_SETPOS32,   0,  val);
}

void Cfg_Open(void) {
    if (g_hCfg && IsWindow(g_hCfg)) { SetForegroundWindow(g_hCfg); return; }

    /* 计算外部窗口尺寸，使客户区刚好容纳控件 */
    RECT rc = {0, 0, 240, 110};
    AdjustWindowRectEx(&rc,
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
        FALSE,
        WS_EX_DLGMODALFRAME|WS_EX_APPWINDOW);

    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int W = rc.right - rc.left;
    int H = rc.bottom - rc.top;
    int cx = (wa.right  + wa.left - W) / 2;
    int cy = (wa.bottom + wa.top  - H) / 2;

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc   = CfgProc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName = WC_CFG;
    wc.hIcon         = g_hIco;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    g_hCfg = CreateWindowExW(
        WS_EX_DLGMODALFRAME|WS_EX_APPWINDOW,
        WC_CFG, APP_NAME L"  —  设置",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
        cx, cy, W, H, NULL, NULL, GetModuleHandleW(NULL), NULL);

    ShowWindow(g_hCfg, SW_SHOW);
    UpdateWindow(g_hCfg);
}

LRESULT CALLBACK CfgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HFONT fT=NULL, fN=NULL, fS=NULL;   /* title / normal / small */

    switch (msg) {
    case WM_CREATE: {
        fT = Util_Font(11, TRUE);
        fN = Util_Font(10, FALSE);
        fS = Util_Font(9,  FALSE);

        /* 统一边距和整齐排列 */
        _Ctrl(L"STATIC", L"工作时长 (分)", SS_LEFT|SS_CENTERIMAGE,
              20, 20, 100, 24, hwnd, 0, fN);
              
        wchar_t v[8];
        swprintf(v,8,L"%d",g_cfg.workMin);
        HWND eW = _Edit(v, 120, 20, 48, 24, hwnd, IDC_WORK_E, fN);
        _Spin(eW, 1, 180, g_cfg.workMin, hwnd, IDC_WORK_S);
        _Ctrl(L"STATIC", L"1-180", SS_LEFT|SS_CENTERIMAGE,
              176, 20, 40, 24, hwnd, 0, fS);

        /* 开机启动 */
        HWND hChk = _Ctrl(L"BUTTON", L"开机自动启动", BS_AUTOCHECKBOX,
                          20, 60, 110, 24, hwnd, IDC_AUTO, fN);
        SendMessageW(hChk, BM_SETCHECK, g_cfg.autoStart ? BST_CHECKED : BST_UNCHECKED, 0);

        /* 保存按钮 */
        _Ctrl(L"BUTTON", L"保存设置", BS_DEFPUSHBUTTON,
              140, 58, 76, 28, hwnd, IDC_OK, fT);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_CANCEL:
            DestroyWindow(hwnd);
            break;
        case IDC_OK: {
            wchar_t v[8];
            GetWindowTextW(GetDlgItem(hwnd,IDC_WORK_E), v,8); int nw=_wtoi(v);
            if (nw<1||nw>180) {
                MessageBoxW(hwnd, L"请检查工作时长输入范围(1-180)", APP_NAME, MB_OK|MB_ICONWARNING);
                return 0;
            }
            g_cfg.workMin = nw;
            g_cfg.autoStart = (BST_CHECKED == SendMessageW(GetDlgItem(hwnd,IDC_AUTO), BM_GETCHECK,0,0));
            Config_Save();
            Logic_StartWork();
            HUD_Refresh();
            Tray_Update();
            DestroyWindow(hwnd);
            break;
        }
        }
        return 0;
    case WM_DESTROY:
        if(fT) { DeleteObject(fT); fT=NULL; }
        if(fN) { DeleteObject(fN); fN=NULL; }
        if(fS) { DeleteObject(fS); fS=NULL; }
        g_hCfg = NULL;
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
