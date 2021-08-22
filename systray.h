#ifndef SYSTRAY_H_FILE
#define SYSTRAY_H_FILE

#include <windows.h>

#define WM_APP_NOTIFY_CALLBACK (WM_APP+1)

void systray_init(HINSTANCE h_inst, HWND hwnd);
void systray_close(HWND hwnd);
void systray_open_menu(HINSTANCE h_inst, HWND hwnd, POINT pt);

#endif /* SYSTRAY_H_FILE */
