#include <stdio.h>
#include <windows.h>
#include <commctrl.h>

#include "network.h"
#include "parse.h"
#include "util.h"
#include "systray.h"
#include "about.h"
#include "settings.h"
#include "resources.h"

#define MAX_INPUT_SEQ_SIZE  100

static HINSTANCE h_instance;
static HWND hwnd_main;
static struct SETTINGS settings;

void debug_msg(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  fflush(stdout);
}

void display_msg(const char *fmt, ...)
{
  static char display_text[65536];
  static char buf[1024];
  va_list ap;

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  size_t buf_size = strlen(buf);
  if (buf_size >= sizeof(display_text)) buf_size = sizeof(display_text)-1;
  size_t cur_size = strlen(display_text);
  size_t cut_pos = 0;
  while (cur_size - cut_pos + buf_size >= sizeof(display_text)) {
    while (cut_pos < cur_size && display_text[cut_pos] != '\n') {
      cut_pos++;
    }
    if (cut_pos < cur_size) cut_pos++;
  }
  memmove(display_text, &display_text[cut_pos], cur_size - cut_pos);
  memcpy(&display_text[cur_size - cut_pos], buf, buf_size);
  display_text[cur_size - cut_pos + buf_size] = '\0';

  PostMessage(hwnd_main, WM_USER, 0, (LPARAM) display_text);
}

static void process_network_message(const char *msg)
{
  DEBUG_MSG("-> processing message '%s'\n", msg);
  
  INPUT input[MAX_INPUT_SEQ_SIZE];
  memset(input, 0, sizeof(input));
  int n_inputs = parse_key_input(input, MAX_INPUT_SEQ_SIZE, msg);
  DEBUG_MSG("-> got %d inputs\n", n_inputs);
  if (n_inputs > 0) {
    SendInput(n_inputs, input, sizeof(INPUT));
  }
}

static LRESULT CALLBACK msg_display_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
  LRESULT ret = DefSubclassProc(hwnd, msg, wParam, lParam);
  HideCaret(hwnd);
  return ret;  
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  static HWND hwnd_msg_display;
  
  switch (msg) {
  case WM_CREATE:
    hwnd_msg_display = CreateWindowEx(0, "EDIT",
                                      NULL,
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                      0, 0, 0, 0,
                                      hwnd,
                                      NULL,
                                      (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
                                      NULL);
    SetWindowSubclass(hwnd_msg_display, msg_display_wnd_proc, 0, 0);
    systray_init(h_instance, hwnd);
    return 0;
    
  case WM_SIZE:
    {
      MoveWindow(hwnd_msg_display, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
      RECT rect = { .left = 3, .top = 3, .right = LOWORD(lParam) - 6, .bottom = HIWORD(lParam) - 6 };
      SendMessage(hwnd_msg_display, EM_SETRECT, 0, (LPARAM) &rect);
    }
    return 0;

  case WM_USER:
    SendMessage(hwnd_msg_display, WM_SETTEXT, 0, lParam);
    SendMessage(hwnd_msg_display, EM_LINESCROLL, 0, strlen((char *) lParam));
    return 0;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDM_EXIT:
      DestroyWindow(hwnd);
      break;

    case IDM_ABOUT:
      about_show_dialog(h_instance, hwnd);
      break;

    case IDM_SETTINGS:
      if (settings_show_dialog(h_instance, hwnd, &settings) != 0) {
        net_update_settings(settings.port);
      }
      break;
    }
    break;
    
  case WM_APP_NOTIFY_CALLBACK:
    switch (LOWORD(lParam)) {
    case NIN_SELECT:
      if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
      } else {
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
      }
      break;

    case WM_CONTEXTMENU:
      {
        POINT pt = { LOWORD(wParam), HIWORD(wParam) };
        systray_open_menu(h_instance, hwnd, pt);
      }
      break;
    }
    return 0;
    
  case WM_CLOSE:
    //DestroyWindow(hwnd);
    ShowWindow(hwnd, SW_HIDE);
    break;
    
  case WM_DESTROY:
    systray_close(hwnd);
    PostQuitMessage(0);
    break;
    
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE h_inst, HINSTANCE h_prev_instance, LPSTR cmdline, int cmd_show)
{
  static const char class_name[] = "RemoteControlClass";
  h_instance = h_inst;
  
  WNDCLASSEX wc;
  wc.cbSize        = sizeof(WNDCLASSEX);
  wc.style         = 0;
  wc.lpfnWndProc   = WndProc;
  wc.cbClsExtra    = 0;
  wc.cbWndExtra    = 0;
  wc.hInstance     = h_inst;
  wc.hIcon         = LoadIcon(h_inst, MAKEINTRESOURCE(IDI_ICON));
  wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  wc.lpszMenuName  = NULL;
  wc.lpszClassName = class_name;
  wc.hIconSm       = LoadIcon(h_inst, MAKEINTRESOURCE(IDI_ICON));

  if (!RegisterClassEx(&wc)) {
    MessageBox(NULL, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
    return 0;
  }

  hwnd_main = CreateWindowEx(WS_EX_CLIENTEDGE|WS_EX_WINDOWEDGE,
                             class_name,
                             "Remote Control",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 400, 220,
                             NULL, NULL, h_inst, NULL);
  if (hwnd_main == NULL) {
    MessageBox(NULL, "Window creation failed", "Error", MB_ICONEXCLAMATION | MB_OK);
    return 0;
  }
  ShowWindow(hwnd_main, SW_HIDE);
  UpdateWindow(hwnd_main);

  settings_read(&settings);
  
  if (net_init(settings.port, process_network_message) != 0) {
    MessageBox(NULL, "Error creating network thread", "Error", MB_ICONEXCLAMATION | MB_OK);
    return 0;
  }
  
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return msg.wParam;
}
