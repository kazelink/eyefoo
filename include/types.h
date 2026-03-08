#ifndef TYPES_H
#define TYPES_H



#include <windows.h>

#define APP_NAME      L"护眼提醒"
#define WC_MAIN       L"EyeMain"
#define WC_HUD        L"EyeHUD"
#define WC_CFG        L"EyeCfg"

#define WM_TRAY       (WM_APP + 1)
#define ID_TICK       1

typedef enum { ST_WORK, ST_PAUSED, ST_FOCUS } State;

typedef struct {
    int workMin;
    int focusMin;
    BOOL autoStart;
} Cfg;

extern Cfg   g_cfg;
extern State g_state;
extern State g_saved_state;
extern int   g_elapsed;
extern int   g_focus_elapsed;
extern int   g_snoozed;
extern BOOL  g_warned;

extern HWND  g_hMain;
extern HWND  g_hHUD;
extern HWND  g_hCfg;
extern HFONT g_hFont;
extern HFONT g_hFontHeavy;
extern HICON g_hIco;

#define DEF_WORK_MIN   40
#define DEF_FOCUS_MIN  60
#define MAX_SNOOZE      3
#define SNOOZE_MIN      5
#define WARN_SEC       60

#endif
