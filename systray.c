#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include "systray.h"
#include "resources.h"
#include "util.h"

void systray_init(HINSTANCE h_inst, HWND hwnd)
{
  NOTIFYICONDATA nid;
  memset(&nid, 0, sizeof(NOTIFYICONDATA));
  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = (HWND) hwnd;
  nid.uID = IDI_SYSTRAY;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
  nid.hIcon = LoadIcon(h_inst, MAKEINTRESOURCE(IDI_ICON));
  LoadString(h_inst, IDS_TOOLTIP, nid.szTip, sizeof(nid.szTip)-1);
  nid.uCallbackMessage = WM_APP_NOTIFY_CALLBACK;
  Shell_NotifyIcon(NIM_ADD, &nid);

  nid.uVersion = NOTIFYICON_VERSION_4;
  Shell_NotifyIcon(NIM_SETVERSION, &nid);
}

void systray_close(HWND hwnd)
{
  NOTIFYICONDATA nid;
  memset(&nid, 0, sizeof(NOTIFYICONDATA));
  nid.cbSize = sizeof(NOTIFYICONDATA);
  nid.hWnd = (HWND) hwnd;
  nid.uID = IDI_SYSTRAY;
  Shell_NotifyIcon(NIM_DELETE, &nid);
}

void systray_open_menu(HINSTANCE h_inst, HWND hwnd, POINT pt)
{
  HMENU h_menu = LoadMenu(h_inst, MAKEINTRESOURCE(IDC_CONTEXTMENU));
  if (! h_menu) return;
  
  HMENU h_sub_menu = GetSubMenu(h_menu, 0);
  if (! h_sub_menu) {
    DestroyMenu(h_menu);
    return;
  }

  SetForegroundWindow(hwnd);
    
  UINT flags = TPM_RIGHTBUTTON;
  if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0) {
    flags |= TPM_RIGHTALIGN;
  } else {
    flags |= TPM_LEFTALIGN;
  }
  
  TrackPopupMenuEx(h_sub_menu, flags, pt.x, pt.y, hwnd, NULL);
  DestroyMenu(h_menu);
}
