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
static BOOL s_idleLogged = FALSE;

static const wchar_t *StateName(State state) {
    switch (state) {
    case ST_WORK:   return L"WORK";
    case ST_PAUSED: return L"PAUSED";
    case ST_FOCUS:  return L"FOCUS";
    default:        return L"UNKNOWN";
    }
}

static const wchar_t *EventName(LogicEvent ev) {
    switch (ev) {
    case EV_TO_WORK_RESET:          return L"TO_WORK_RESET";
    case EV_TO_WORK_RESUME:         return L"TO_WORK_RESUME";
    case EV_TO_PAUSED_WORK_TIMEOUT: return L"TO_PAUSED_WORK_TIMEOUT";
    case EV_TO_FOCUS_START:         return L"TO_FOCUS_START";
    case EV_SESSION_LOCK:           return L"SESSION_LOCK";
    case EV_SESSION_UNLOCK:         return L"SESSION_UNLOCK";
    default:                        return L"UNKNOWN_EVENT";
    }
}

static void ResetWorkCycle(void) {
    g_elapsed = 0;
    g_warned = FALSE;
    g_snoozed = 0;
}

static void Logic_Transit(LogicEvent ev) {
    State oldState = g_state;

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

    Util_LogFormat(
        L"State transition: event=%ls, %ls -> %ls, elapsed=%d, focusElapsed=%ld, snoozed=%d",
        EventName(ev),
        StateName(oldState),
        StateName(g_state),
        g_elapsed,
        g_focus_elapsed,
        g_snoozed);
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
    HANDLE hThread;

    if (InterlockedCompareExchange(&s_beepRunning, 1, 0) != 0) {
        return;
    }

    hThread = CreateThread(NULL, 0, WarningBeepThreadProc, NULL, 0, NULL);
    if (!hThread) {
        InterlockedExchange(&s_beepRunning, 0);
        Util_LogLastError(L"CreateThread(WarningBeepThreadProc)");
        return;
    }
    CloseHandle(hThread);
}

void Logic_StartWork(void) {
    Util_Log(L"Work cycle started");
    Logic_Transit(EV_TO_WORK_RESET);
}

void Logic_StartRest(void) {
    Util_LogFormat(L"Work interval reached. Attempting to lock workstation at elapsed=%d", g_elapsed);
    Logic_Transit(EV_TO_PAUSED_WORK_TIMEOUT);
    if (!LockWorkStation()) {
        Util_LogLastError(L"LockWorkStation");
        Logic_Transit(EV_TO_WORK_RESET);
    } else {
        Util_Log(L"LockWorkStation succeeded");
    }
}

void Logic_StartFocus(void) {
    wchar_t msg[128];

    Util_LogFormat(L"Focus mode started for %d minutes", g_cfg.focusMin);
    Logic_Transit(EV_TO_FOCUS_START);
    if (g_cfg.focusMin >= 60 && g_cfg.focusMin % 60 == 0) {
        swprintf(msg, 128, L"Focus mode will suspend eye reminders for %d hour(s).", g_cfg.focusMin / 60);
    } else {
        swprintf(msg, 128, L"Focus mode will suspend eye reminders for %d minute(s).", g_cfg.focusMin);
    }
    Tray_Balloon(L"Focus mode enabled", msg);
    HUD_Refresh();
    Tray_Update();
}

void Logic_StopFocus(void) {
    Util_LogFormat(L"Focus mode stopped manually at focusElapsed=%ld", g_focus_elapsed);
    Logic_Transit(EV_TO_WORK_RESUME);
    Tray_Balloon(L"Focus mode ended", L"Normal eye reminder cycle has resumed.");
    HUD_Refresh();
    Tray_Update();
}

void Logic_Reset(void) {
    if (g_state == ST_WORK) {
        Util_LogFormat(L"Manual reset at elapsed=%d", g_elapsed);
        ResetWorkCycle();
        HUD_Refresh();
        Tray_Update();
    }
}

void Logic_Snooze(void) {
    if (g_state != ST_WORK || g_snoozed >= MAX_SNOOZE) {
        return;
    }

    g_snoozed++;
    g_elapsed -= SNOOZE_MIN * 60;
    if (g_elapsed < 0) {
        g_elapsed = 0;
    }
    g_warned = FALSE;
    Util_LogFormat(L"Snoozed by %d minutes. snoozed=%d, elapsed=%d", SNOOZE_MIN, g_snoozed, g_elapsed);
    HUD_Refresh();
    Tray_Update();
}

void Logic_Tick(void) {
    if (g_state == ST_PAUSED) {
        return;
    }

    {
        QUERY_USER_NOTIFICATION_STATE qstate;
        if (SUCCEEDED(SHQueryUserNotificationState(&qstate))) {
            if (qstate == QUNS_BUSY || qstate == QUNS_RUNNING_D3D_FULL_SCREEN || qstate == QUNS_PRESENTATION_MODE) {
                if (g_hHUD && IsWindowVisible(g_hHUD)) {
                    ShowWindow(g_hHUD, SW_HIDE);
                }
            } else {
                if (g_hHUD && !IsWindowVisible(g_hHUD)) {
                    ShowWindow(g_hHUD, SW_SHOWNOACTIVATE);
                }
            }
        }
    }

    {
        LASTINPUTINFO lii;
        lii.cbSize = sizeof(LASTINPUTINFO);
        if (GetLastInputInfo(&lii)) {
            DWORD idleMs = GetTickCount() - lii.dwTime;
            if (idleMs > 30000) {
                if (!g_is_idle) {
                    g_is_idle = TRUE;
                    if (!s_idleLogged) {
                        Util_LogFormat(L"User idle detected. idleMs=%lu", (unsigned long)idleMs);
                        s_idleLogged = TRUE;
                    }
                    HUD_Refresh();
                    Tray_Update();
                }
                return;
            }

            if (g_is_idle) {
                g_is_idle = FALSE;
                if (s_idleLogged) {
                    Util_LogFormat(L"User activity resumed after idle. idleMs=%lu", (unsigned long)idleMs);
                    s_idleLogged = FALSE;
                }
            }
        }
    }

    if (g_state == ST_FOCUS) {
        int rem;

        g_focus_elapsed++;
        rem = g_cfg.focusMin * 60 - g_focus_elapsed;
        if (rem <= 0) {
            Util_Log(L"Focus mode completed; resuming work cycle");
            Tray_Balloon(L"Focus mode ended", L"Focus time is up. Normal reminders resumed.");
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
            Util_LogFormat(L"Work warning threshold reached. remaining=%d", rem);
            Tray_Balloon(L"Break soon", L"Screen lock will trigger in about 1 minute.");
        }
        if (rem <= 0) {
            Logic_StartRest();
        }
    }

    HUD_Refresh();
    Tray_Update();
}

void Logic_OnSessionLock(void) {
    Util_Log(L"Session lock notification received");
    Logic_Transit(EV_SESSION_LOCK);
    HUD_Refresh();
    Tray_Update();
}

void Logic_OnSessionUnlock(void) {
    Util_Log(L"Session unlock notification received");
    Logic_Transit(EV_SESSION_UNLOCK);
    HUD_Refresh();
    Tray_Update();
}
