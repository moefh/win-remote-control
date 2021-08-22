#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <windows.h>

#include "network.h"
#include "util.h"

DWORD WINAPI net_main(LPVOID lpParam)
{
  struct NET_MAIN_ARGS *args = (struct NET_MAIN_ARGS *) lpParam;
  
  WORD wsa_version_wanted = MAKEWORD(1, 1);
  WSADATA wsa_data;
  WSAStartup(wsa_version_wanted, &wsa_data);
  
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    display_msg("NETWORK ERROR: can't create socket");
    return 0;
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port   = htons(args->server_port);
  server.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *) &server, sizeof(server)) == -1) {
    display_msg("NETWORK ERROR: can't bind socket");
    return 0;
  }

  if (listen(sock, 5) == -1) {
    display_msg("NETWORK ERROR: can't listen on socket");
    return 0;
  }

  display_msg("Accepting connections on port %d", args->server_port);
  
  while (1) {
    struct sockaddr_in client;
    int client_size = sizeof(client);
    int client_fd = accept(sock, (struct sockaddr *) &client, &client_size);
    if (client_fd == -1) {
      display_msg("NETWORK ERROR: can't accept connection");
      continue;
    }

    char buf[1024];
    int len = recv(client_fd, buf, sizeof(buf), 0);
    if (len >= 0) {
      if (len >= sizeof(buf)) len = sizeof(buf)-1;
      buf[len] = '\0';
      args->msg_proc(buf);
    }

    closesocket(client_fd);
  }
  
  return 0;
}
