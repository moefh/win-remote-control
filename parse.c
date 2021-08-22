#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "parse.h"
#include "util.h"

struct KEY_DEF {
  char name[8];
  WORD vk;
} key_defs[] = {
  { "SHIFT",   VK_SHIFT      },
  { "CTRL",    VK_CONTROL    },
  { "ALT",     VK_MENU       },
  { "BACK",    VK_BACK       },
  { "TAB",     VK_TAB        },

  { "ENTER",   VK_RETURN     },
  { "CAPS",    VK_CAPITAL    },
  { "ESC",     VK_ESCAPE     },
  { "SPACE",   VK_SPACE      },
  { "PGUP",    VK_PRIOR      },
  { "PGDN",    VK_NEXT       },
  { "END",     VK_END        },
  { "HOME",    VK_HOME       },
  { "LEFT",    VK_LEFT       },
  { "UP",      VK_UP         },
  { "RIGHT",   VK_RIGHT      },
  { "DOWN",    VK_DOWN       },

  { "INS",     VK_INSERT     },
  { "DEL",     VK_DELETE     },
  { "LWIN",    VK_LWIN       },
  { "RWIN",    VK_RWIN       },
  { "MENU",    VK_APPS       },

  { "BRA",     VK_OEM_4      },
  { "KET",     VK_OEM_6      },
  { "SLASH",   VK_OEM_2      },
  { "BKSLASH", VK_OEM_5      },
  { "TILDE",   VK_OEM_3      },
  { "COLON",   VK_OEM_1      },
  { "QUOTE",   VK_OEM_7      },
  { "PLUS",    VK_OEM_PLUS   },
  { "MINUS",   VK_OEM_MINUS  },
  { "COMMA",   VK_OEM_COMMA  },
  { "DOT",     VK_OEM_PERIOD },
  
  { "NUM0",    VK_NUMPAD0    },
  { "NUM1",    VK_NUMPAD1    },
  { "NUM2",    VK_NUMPAD2    },
  { "NUM3",    VK_NUMPAD3    },
  { "NUM4",    VK_NUMPAD4    },
  { "NUM5",    VK_NUMPAD5    },
  { "NUM6",    VK_NUMPAD6    },
  { "NUM7",    VK_NUMPAD7    },
  { "NUM8",    VK_NUMPAD8    },
  { "NUM9",    VK_NUMPAD9    },
  
  { "F1",      VK_F1         },
  { "F2",      VK_F2         },
  { "F3",      VK_F3         },
  { "F4",      VK_F4         },
  { "F5",      VK_F5         },
  { "F6",      VK_F6         },
  { "F7",      VK_F7         },
  { "F8",      VK_F8         },
  { "F9",      VK_F9         },
  { "F10",     VK_F10        },
  { "F11",     VK_F11        },
  { "F12",     VK_F12        },
};

static int parse_key(WORD *key, const char *name, int name_len)
{
  if (name_len == 1) {
    if ((*name >= 'A' && *name <= 'Z') || (*name >= '0' && *name <= '9')) {
      *key = *name;
      DEBUG_MSG("-> adding simple key press for %c: %d\n", *name, *name);
      return 1;
    }
  }
  
  if (name_len > sizeof(key_defs[0].name)) return 0;
  
  for (int i = 0; i < sizeof(key_defs)/sizeof(key_defs[0]); i++) {
    struct KEY_DEF *kd = &key_defs[i];
    if (memcmp(kd->name, name, name_len) == 0 && kd->name[name_len] == '\0') {
      *key = kd->vk;
      DEBUG_MSG("-> adding key press for '%s': %d\n", kd->name, (int) kd->vk);
      return 1;
    }
  }
  return 0;
}

static void release_keys(INPUT *input, int start, int num_release)
{
  for (int i = 0; i < num_release; i++) {
    INPUT *release = &input[start + i];
    INPUT *press = &input[start - 1 - i];
    release->type = INPUT_KEYBOARD;
    release->ki.wVk = press->ki.wVk;
    release->ki.dwFlags = KEYEVENTF_KEYUP;
    DEBUG_MSG("-> adding key release %d\n", (int) release->ki.wVk);
  }
}

static int is_space(char c)
{
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

int parse_key_input(INPUT *input, int max_inputs, const char *text)
{
  int num_inputs = 0;
  int num_combined = 0;

  display_msg("\r\n-> %s", text);
  
  const char *p = text;
  while (*p != '\0') {
    while (is_space(*p) || *p == '+') {
      if (is_space(*p)) num_combined = 0;
      p++;
    }
    if (*p == '\0') {
      break;
    }

    if (num_inputs + 1 > max_inputs) {
      display_msg("\r\nERROR: too many keys");
      return 0;
    }
    
    const char *start = p;
    while (! is_space(*p) && *p != '+' && *p != '\0') p++;
    int len = p - start;
    if (! parse_key(&input[num_inputs].ki.wVk, start, len)) {
      display_msg("\r\nERROR: invalid key name: '%.*s'", len, start);
      return 0;
    }
    input[num_inputs].type = INPUT_KEYBOARD;
    if (++num_inputs >= max_inputs) {
      break;
    }

    if (is_space(*p) || *p == '\0') {
      if (num_inputs + num_combined + 1 > max_inputs) {
        display_msg("\r\nERROR: too many keys");
        return 0;
      }
      release_keys(input, num_inputs, num_combined+1);
      num_inputs += num_combined + 1;
      num_combined = 0;
    } else {
      num_combined++;
    }
  }

  if (num_combined > 0) {
    display_msg("\r\nERROR: malformed input");
    return 0;
  }

  return num_inputs;
}
