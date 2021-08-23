#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "settings.h"
#include "resources.h"
#include "util.h"

#define DEFAULT_PORT 5555

static int get_config_filename(char *filename, size_t size)
{
  const char ext[] = ".cfg";
  
  GetModuleFileName(NULL, filename, size);
  char *last_slash = strrchr(filename, '\\');
  char *last_dot = strrchr(filename, '.');
  if (last_dot != NULL && (last_slash == NULL || last_dot > last_slash) && last_dot - filename + sizeof(ext) < size) {
    memcpy(last_dot, ext, sizeof(ext));
    return 0;
  }

  return 1;
}

static int parse_port(char *data)
{
  char *end;
  unsigned long port = strtoul(data, &end, 0);
  if (end == NULL || *end != '\0' || port == 0 || port > 65535) {
    return -1;
  }
  return (int) port;
}

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
        char data[64];
        GetDlgItemTextA(hwnd, IDC_SETTINGS_PORT, data, sizeof(data));
        int port = parse_port(data);
        if (port < 0) {
          MessageBox(hwnd, "Invalid port number", "Error", MB_OK|MB_ICONERROR);
          return TRUE;
        }
        if (settings->port == port) {
          EndDialog(hwnd, 0);
          return TRUE;
        }
        
        settings->port = port;
        settings_write(settings);
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

  char cfg_filename[1024];
  if (get_config_filename(cfg_filename, sizeof(cfg_filename)) != 0) {
    return;
  }
  DEBUG_MSG("-> reading config file: '%s'\n", cfg_filename);
  FILE *f = fopen(cfg_filename, "r");
  if (f == NULL) {
    DEBUG_MSG("-> can't open config file\n");
    return;
  }

  char line[1024];
  while (fgets(line, sizeof(line), f) != NULL) {
    char *opt = line;
    while (*opt == ' ' || *opt == '\t' || *opt == '\n') opt++;
    if (*opt == '\0') continue;
    
    char *opt_end = opt;
    while (*opt_end != ' ' && *opt_end != '\t' && *opt_end != '\n' && *opt_end != '=' && *opt_end != '\0') opt_end++;
    if (*opt_end == '\0') continue;
    int found_eq = *opt_end == '=';
    *opt_end = '\0';
    
    char *val = opt_end + 1;
    if (! found_eq) {
      while (*val != '=' && *val != '\0') val++;
      if (*val == '=') val++;
    }
    while (*val == ' ' || *val == '\t' || *val == '\n') val++;
    
    char *val_end = val;
    while (*val_end != '\n' && *val_end != '\0') val_end++;
    *val_end = '\0';

    DEBUG_MSG("-> got config option '%s' = '%s'\n", opt, val);
    
    if (strcmp(opt, "server_port") == 0) {
      int port = parse_port(val);
      if (port > 0) {
        settings->port = port;
      }
      continue;
    }

    // ignore other options
  }
  
  fclose(f);
}

void settings_write(struct SETTINGS *settings)
{
  char cfg_filename[1024];
  if (get_config_filename(cfg_filename, sizeof(cfg_filename)) != 0) {
    return;
  }

  DEBUG_MSG("-> writing config file: '%s'\n", cfg_filename);
  FILE *f = fopen(cfg_filename, "w");
  if (f == NULL) {
    return;
  }

  fprintf(f, "server_port = %d\n", settings->port);
  
  fclose(f);
}
