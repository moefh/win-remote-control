#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

#include "settings.h"
#include "resources.h"

#define DEFAULT_PORT 5555

static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  static struct SETTINGS *settings;
  
  switch (msg) {
  case WM_INITDIALOG:
    {
      settings = (struct SETTINGS *) lParam;
      char data[64];
      snprintf(data, sizeof(data), "%d", settings->port);
      SetDlgItemText(hwnd, IDC_SETTINGS_PORT, data);
    }
    return TRUE;
    
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDCANCEL:
    case IDCLOSE:
      EndDialog(hwnd, 0);
      return TRUE;

    case IDOK:
      {
        char data[64], *end;
        GetDlgItemTextA(hwnd, IDC_SETTINGS_PORT, data, sizeof(data));
        unsigned long port = strtoul(data, &end, 0);
        if (end == NULL || *end != '\0' || port == 0 || port > 65535) {
          MessageBox(hwnd, "Invalid port number", "Error", MB_OK|MB_ICONERROR);
          return TRUE;
        }
        settings->port = (int) port;
        EndDialog(hwnd, 1);
      }
      return TRUE;
    }
    break;
  }
  return FALSE;
}

int settings_show_dialog(HINSTANCE h_inst, HWND hwnd, struct SETTINGS *settings)
{
  return DialogBoxParam(h_inst, MAKEINTRESOURCE(IDD_SETTINGS), hwnd, DlgProc, (LPARAM) settings) != 0;
}

void settings_read(struct SETTINGS *settings)
{
  settings->port = DEFAULT_PORT;
}
