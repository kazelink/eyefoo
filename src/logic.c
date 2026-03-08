#include "logic.h"
#include "types.h"
#include "hud.h"
#include "tray.h"
#include "utils.h"
#include <stdio.h>
#include <shellapi.h>

typedef enum {
    EV_TO_WORK_RESET,
    EV_TO_WORK_RESUME,
    EV_TO_PAUSED_WORK_TIMEOUT,
    EV_TO_FOCUS_START,
    EV_SESSION_LOCK,
    EV_SESSION_UNLOCK
} LogicEvent;

static volatile LONG s_beepRunning = 0;

static void ResetWorkCycle(void) {
    g_elapsed = 0;
    g_warned = FALSE;
    g_snoozed = 0;
}

static void Logic_Transit(LogicEvent ev) {
    switch (ev) {
    case EV_TO_WORK_RESET:
        g_state = ST_WORK;
        g_focus_elapsed = 0;
        ResetWorkCycle();
        break;
    case EV_TO_WORK_RESUME:
        g_state = ST_WORK;
        g_focus_elapsed = 0;
        break;
    case EV_TO_PAUSED_WORK_TIMEOUT:
        g_saved_state = ST_WORK;
        g_state = ST_PAUSED;
        break;
    case EV_TO_FOCUS_START:
        g_state = ST_FOCUS;
        g_focus_elapsed = 0;
        g_warned = FALSE;
        break;
    case EV_SESSION_LOCK:
        if (g_state != ST_PAUSED) {
            g_saved_state = g_state;
            g_state = ST_PAUSED;
        } else {
            g_saved_state = ST_WORK;
        }
        break;
    case EV_SESSION_UNLOCK:
        if (g_saved_state == ST_FOCUS) {
            g_state = ST_FOCUS;
        } else {
            g_state = ST_WORK;
            ResetWorkCycle();
        }
        break;
    }
}

static DWORD WINAPI WarningBeepThreadProc(LPVOID unused) {
    const DWORD gapMs = 90;
    (void)unused;
    Beep(620, 120);
    Sleep(gapMs);
    Beep(620, 120);
    Sleep(gapMs);
    Beep(620, 120);
    InterlockedExchange(&s_beepRunning, 0);
    return 0;
}

static void PlayGentleWarningTripleBeepAsync(void) {
    if (InterlockedCompareExchange(&s_beepRunning, 1, 0) != 0) {
        return;
    }

    HANDLE hThread = CreateThread(NULL, 0, WarningBeepThreadProc, NULL, 0, NULL);
    if (!hThread) {
        InterlockedExchange(&s_beepRunning, 0);
        Util_LogLastError(L"CreateThread(WarningBeepThreadProc)");
        return;
    }
    CloseHandle(hThread);
}

void Logic_StartWork(void) {
    Logic_Transit(EV_TO_WORK_RESET);
}

void Logic_StartRest(void) {
    Logic_Transit(EV_TO_PAUSED_WORK_TIMEOUT);
    if (!LockWorkStation()) {
        Util_LogLastError(L"LockWorkStation");
        Logic_Transit(EV_TO_WORK_RESET);
    }
}

void Logic_StartFocus(void) {
    Logic_Transit(EV_TO_FOCUS_START);
    wchar_t msg[128];
    if (g_cfg.focusMin >= 60 && g_cfg.focusMin % 60 == 0) {
        swprintf(msg, 128, L"接下来的 %d 小时护眼提醒将被挂起。", g_cfg.focusMin / 60);
    } else {
        swprintf(msg, 128, L"接下来的 %d 分钟护眼提醒将被挂起。", g_cfg.focusMin);
    }
    Tray_Balloon(L"专注模式已开启", msg);
    HUD_Refresh();
    Tray_Update();
}

void Logic_StopFocus(void) {
    Logic_Transit(EV_TO_WORK_RESUME);
    Tray_Balloon(L"专注结束", L"已手动结束专注模式，恢复常规护眼循环");
    HUD_Refresh();
    Tray_Update();
}

void Logic_Reset(void) {
    if (g_state == ST_WORK) {
        g_elapsed = 0;
        g_warned = FALSE;
        g_snoozed = 0;
        HUD_Refresh();
        Tray_Update();
    }
}

void Logic_Snooze(void) {
    if (g_state != ST_WORK || g_snoozed >= MAX_SNOOZE) return;
    g_snoozed++;
    g_elapsed -= SNOOZE_MIN * 60;
    if (g_elapsed < 0) g_elapsed = 0;
    g_warned = FALSE;
    HUD_Refresh();
    Tray_Update();
}

void Logic_Tick(void) {
    if (g_state == ST_PAUSED) return;

    QUERY_USER_NOTIFICATION_STATE qstate;
    if (SUCCEEDED(SHQueryUserNotificationState(&qstate))) {
        if (qstate == QUNS_BUSY || qstate == QUNS_RUNNING_D3D_FULL_SCREEN || qstate == QUNS_PRESENTATION_MODE) {
            if (g_hHUD && IsWindowVisible(g_hHUD)) {
                ShowWindow(g_hHUD, SW_HIDE);
            }
            Tray_Update();
            return;
        } else {
            if (g_hHUD && !IsWindowVisible(g_hHUD)) {
                ShowWindow(g_hHUD, SW_SHOWNOACTIVATE);
            }
        }
    }

    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);
    if (GetLastInputInfo(&lii)) {
        DWORD idleMs = GetTickCount() - lii.dwTime;
        if (idleMs > 30000) {
            // Treat as paused when user is idle for more than 30s
            // Don't update g_state permanently to avoid breaking manual resume
            // We just return out so `g_elapsed` doesn't increase.
            g_is_idle = TRUE;
            HUD_Refresh();
            Tray_Update();
            return;
        } else {
            g_is_idle = FALSE;
        }
    }

    if (g_state == ST_FOCUS) {
        g_focus_elapsed++;
        int rem = g_cfg.focusMin * 60 - g_focus_elapsed;
        if (rem <= 0) {
            Tray_Balloon(L"专注结束", L"时限已到，已恢复常规护眼循环");
            Logic_Transit(EV_TO_WORK_RESUME);
        }
    } else {
        g_elapsed++;
    }

    if (g_state == ST_WORK) {
        int rem = g_cfg.workMin * 60 - g_elapsed;
        if (rem == 15) {
            PlayGentleWarningTripleBeepAsync();
        }
        if (!g_warned && rem <= WARN_SEC) {
            g_warned = TRUE;
            Tray_Balloon(L"即将休息", L"1 分钟后将自动锁屏");
        }
        if (rem <= 0) Logic_StartRest();
    }

    HUD_Refresh();
    Tray_Update();
}

void Logic_OnSessionLock(void) {
    Logic_Transit(EV_SESSION_LOCK);
    HUD_Refresh();
    Tray_Update();
}

void Logic_OnSessionUnlock(void) {
    Logic_Transit(EV_SESSION_UNLOCK);
    HUD_Refresh();
    Tray_Update();
}
