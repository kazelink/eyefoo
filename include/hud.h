#ifndef HUD_HEADER_H
#define HUD_HEADER_H

#include <windows.h>

void HUD_Create(void);
void HUD_Refresh(void);
LRESULT CALLBACK HUDProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

#endif
