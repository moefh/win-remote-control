
#include <windows.h>

#include "about.h"
#include "resources.h"

static LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_ABOUT_CLOSE:
      EndDialog(hwnd, 0);
      break;
    }
    break;
  }
  return 0;
}

void about_show(HINSTANCE h_inst, HWND hwnd)
{
  DialogBoxW(h_inst, MAKEINTRESOURCEW(IDD_ABOUT), hwnd, DlgProc);
}
