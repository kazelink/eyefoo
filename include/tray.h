#ifndef TRAY_H
#define TRAY_H

#include <windows.h>

void Tray_Init(HWND hwnd);
void Tray_Update(void);
void Tray_Balloon(const wchar_t *title, const wchar_t *msg);
void Tray_Menu(HWND hwnd);

#endif
