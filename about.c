
#include <windows.h>

#include "about.h"
#include "resources.h"

static INT_PTR CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_ABOUT_CLOSE:
    case IDCANCEL:
    case IDCLOSE:
      EndDialog(hwnd, 0);
      return TRUE;
    }
    break;
  }
  return FALSE;
}

void about_show(HINSTANCE h_inst, HWND hwnd)
{
  DialogBox(h_inst, MAKEINTRESOURCE(IDD_ABOUT), hwnd, DlgProc);
}
