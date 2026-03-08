#include "logic.h"
#include "types.h"
#include "hud.h"
#include "tray.h"

void Logic_StartWork(void) {
    g_state   = ST_WORK;
    g_elapsed = 0;
    g_warned  = FALSE;
}

void Logic_StartRest(void) {
    g_saved_state = ST_WORK;
    g_state = ST_PAUSED;
    LockWorkStation();
}

void Logic_StartFocus(void) {
    g_state = ST_FOCUS;
    g_focus_elapsed = 0;
    g_warned = FALSE;
    Tray_Balloon(L"专注模式已开启", L"接下来的 2 小时护眼提醒将被挂起。");
    HUD_Refresh();
    Tray_Update();
}

void Logic_StopFocus(void) {
    g_state = ST_WORK;
    g_focus_elapsed = 0;
    Tray_Balloon(L"专注结束", L"已手动结束专注模式，恢复常规护眼循环");
    HUD_Refresh();
    Tray_Update();
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

    if (g_state == ST_FOCUS) {
        g_focus_elapsed++;
        int rem = FOCUS_MIN * 60 - g_focus_elapsed;
        if (rem <= 0) {
            Tray_Balloon(L"专注结束", L"时限已到，已恢复常规护眼循环");
            g_state = ST_WORK;
            g_focus_elapsed = 0;
        }
    } else {
        g_elapsed++;
    }

    if (g_state == ST_WORK) {
        int rem = g_cfg.workMin * 60 - g_elapsed;
        if (!g_warned && rem <= WARN_SEC) {
            g_warned = TRUE;
            Tray_Balloon(L"即将休息", L"1 分钟后将自动锁屏");
        }
        if (rem <= 0) Logic_StartRest();
    }

    HUD_Refresh();
    Tray_Update();
}
