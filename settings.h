#ifndef SETTINGS_H_FILE
#define SETTINGS_H_FILE

#include <windows.h>

struct SETTINGS {
  int port;
};

void settings_read(struct SETTINGS *settings);
int settings_show_dialog(HINSTANCE h_inst, HWND hwnd, struct SETTINGS *settings);

#endif /* SETTINGS_H_FILE */
