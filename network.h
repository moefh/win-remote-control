#ifndef NETWORK_H_FILE
#define NETWORK_H_FILE

#include <windows.h>

typedef void (*fn_msg_proc)(const char *msg);

struct NET_MAIN_ARGS {
  int server_port;
  fn_msg_proc msg_proc;
};

DWORD WINAPI net_main(LPVOID lpParam);

#endif /* NETWORK_H_FILE */
