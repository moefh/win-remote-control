#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windows.h>

#include "network.h"
#include "util.h"

#define XMALLOC(x)     HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(x))
#define XFREE(x)       do { if ((x) != NULL) { HeapFree(GetProcessHeap(),0,(x)); (x) = NULL; }} while (0)
#define CLOSESOCK(s)   do { if ((s) >= 0) { closesocket(s); (s) = -1; }} while (0)
#define CLOSEEVENT(ev) do { if ((ev) != WSA_INVALID_EVENT) { WSACloseEvent(ev); (ev) = WSA_INVALID_EVENT; }} while (0)

#define MAX_CONNECTIONS  5
#define MAX_SOCKETS      (1+MAX_CONNECTIONS)

struct SOCK_INFO {
  WSAOVERLAPPED *overlap;
  SOCKET sock;
  SOCKET accept_sock;
  WSABUF wsa_buf;
  char buf[1024];
  int buf_len;
};

static LPFN_ACCEPTEX pAcceptEx;

static fn_msg_proc msg_proc_func;
static int server_port;

static int num_sockets;
static WSAEVENT events[MAX_SOCKETS+1];
static struct SOCK_INFO socks[MAX_SOCKETS];
static WSAEVENT reconfig_event;

static int get_acceptex_ptr(int sock)
{
  GUID acceptex_guid = WSAID_ACCEPTEX;
  DWORD bytes;
  int ret = WSAIoctl(sock,
                     SIO_GET_EXTENSION_FUNCTION_POINTER,
                     &acceptex_guid,
                     sizeof(acceptex_guid),
                     &pAcceptEx,
                     sizeof(pAcceptEx),
                     &bytes,
                     NULL,
                     NULL
                     );
  return (ret == SOCKET_ERROR) ? 1 : 0;
}

static int create_listen_socket(void)
{
  int sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (sock == -1) {
    display_msg("NETWORK ERROR: can't create socket");
    return -1;
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port   = htons(server_port);
  server.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *) &server, sizeof(server)) == -1) {
    display_msg("NETWORK ERROR: can't bind socket");
    return -1;
  }

  if (listen(sock, 5) == -1) {
    display_msg("NETWORK ERROR: can't listen on socket");
    return -1;
  }
  
  display_msg("\r\nAccepting connections on port %d", server_port);
  return sock;
}

static void start_accept(int listen_sock)
{
  events[0] = WSACreateEvent();
  if (events[0] == WSA_INVALID_EVENT) {
    DEBUG_MSG("ERROR in WSACreateEvent(): %d\n", WSAGetLastError());
  }

  socks[0].sock = listen_sock;
  socks[0].accept_sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  socks[0].overlap = XMALLOC(sizeof(WSAOVERLAPPED));
  memset(socks[0].overlap, 0, sizeof(WSAOVERLAPPED));
  socks[0].overlap->hEvent = events[0];
  if (num_sockets < 1) num_sockets = 1;

  DWORD bytes_read = 0;
  pAcceptEx(listen_sock,
            socks[0].accept_sock,
            socks[0].buf,
            0,
            sizeof(SOCKADDR_STORAGE)+16,
            sizeof(SOCKADDR_STORAGE)+16,
            &bytes_read,
            socks[0].overlap
            );
}

static void remove_socket(int sock_num)
{
  CLOSESOCK(socks[sock_num].sock);
  XFREE(socks[sock_num].overlap);
  CLOSEEVENT(events[sock_num]);
  
  for (int i = sock_num; i < num_sockets-1; i++) {
    events[i] = events[i+1];
    socks[i] = socks[i+1];
  }
  num_sockets--;
}

static int wait_event(void)
{
  // Shuffle sockets (other than 0) to ensure
  // they all have a chance to be handled.
  if (num_sockets > 2) {
    WSAEVENT first_event = events[1];
    struct SOCK_INFO first_sock = socks[1];
    for (int i = 2; i < num_sockets-1; i++) {
      events[i] = events[i+1];
      socks[i] = socks[i+1];
    }
    events[num_sockets-1] = first_event;
    socks[num_sockets-1] = first_sock;
  }

  // add reconfiguration event to the list of events to be waited for
  const int num_events = num_sockets + 1;
  events[num_events-1] = reconfig_event;
  
  DEBUG_MSG("-> waiting for %d events...\n", num_events);
  int ret = WSAWaitForMultipleEvents(num_events,
                                     events,
                                     FALSE,
                                     WSA_INFINITE, 
                                     FALSE
                                     );
  if (ret == WSA_WAIT_FAILED) {
    DEBUG_MSG("ERROR in WSAWaitForMultipleEvents(): %d\n", WSAGetLastError());
    return -1;
  }
  DEBUG_MSG("-> got event %d\n", (int) (ret - WSA_WAIT_EVENT_0));
  return (ret - WSA_WAIT_EVENT_0);  
}

static void handle_event(int event_num)
{
  DEBUG_MSG("-> handling event %d (%s)\n", event_num, (event_num == num_sockets) ? "reconfiguration" : "socket");
  WSAResetEvent(events[event_num]);       // mark event processed
  if (event_num == num_sockets) return;   // it was a reconfiguration event

  int sock_num = event_num;
  struct SOCK_INFO *cur_sock = &socks[sock_num];

  // read event result
  DWORD bytes_read = 0;
  DWORD flags = 0;
  int ret = WSAGetOverlappedResult(cur_sock->sock, cur_sock->overlap, &bytes_read, FALSE, &flags);
  if (ret == SOCKET_ERROR) {
    DWORD last_err = WSAGetLastError();
    if (last_err != WSA_IO_PENDING) {
      DEBUG_MSG("ERROR in WSAGetOverlappedResult() for socket %d: %d\n", sock_num, WSAGetLastError());
      remove_socket(sock_num);
    }
  }

  // new connection accepted
  if (sock_num == 0) {
    if (num_sockets >= MAX_SOCKETS) { // no space for new connections
      CLOSESOCK(cur_sock->accept_sock);
      return;
    }

    // prepare socket for use
    setsockopt(cur_sock->accept_sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&cur_sock->sock, sizeof(cur_sock->sock));
    shutdown(cur_sock->accept_sock, SD_SEND);  // we'll never send anything
    
    // add event and socket
    int new_sock_num = num_sockets++;
    events[new_sock_num] = cur_sock->overlap->hEvent;
    struct SOCK_INFO *new_sock = &socks[new_sock_num];
    new_sock->sock        = cur_sock->accept_sock;
    new_sock->overlap     = cur_sock->overlap;
    new_sock->buf_len     = 0;
    memset(new_sock->buf, 0, sizeof(new_sock->buf));

    // proceed with new socket
    bytes_read = 0;
    cur_sock = new_sock;
    sock_num = new_sock_num;
  }

  // connected socket closed
  if ((event_num != 0) && (bytes_read == 0)) {
    cur_sock->buf[cur_sock->buf_len] = '\0';
    msg_proc_func(cur_sock->buf);
    remove_socket(sock_num);
    return;
  }
  
  // add received data
  DEBUG_MSG("-> added %d bytes\n", (int) bytes_read);
  cur_sock->buf_len += bytes_read;
  if (cur_sock->buf_len+1 >= sizeof(cur_sock->buf)) {  // buffer full
    cur_sock->buf[cur_sock->buf_len] = '\0';
    msg_proc_func(cur_sock->buf);
    remove_socket(sock_num);
    return;
  }

  // receive more data
  flags = 0;
  cur_sock->wsa_buf.buf = cur_sock->buf + cur_sock->buf_len;
  cur_sock->wsa_buf.len = sizeof(cur_sock->buf) - 1 - cur_sock->buf_len;
  ret = WSARecv(cur_sock->sock, &cur_sock->wsa_buf, 1, NULL, &flags, cur_sock->overlap, NULL);
  if (ret == SOCKET_ERROR) {
    DWORD last_err = WSAGetLastError();
    if (last_err != WSA_IO_PENDING) {
      if (last_err == WSAECONNRESET) {
        // connection closed by remote end
        cur_sock->buf[cur_sock->buf_len] = '\0';
        msg_proc_func(cur_sock->buf);
      }
      remove_socket(sock_num);
    }
  }
}

static DWORD WINAPI net_main(LPVOID lpParam)
{
  // buffer size sanity check
  if (sizeof(socks[0].buf) < 2*sizeof(SOCKADDR_STORAGE) + 32) {
    display_msg("CONFIGURATION ERROR: buffer size is too small");
    return 0;
  }
  
  display_msg("Starting...");

  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);

  for (int i = 0; i < MAX_SOCKETS; i++) {
    events[i] = WSA_INVALID_EVENT;
    socks[i].sock = -1;
    socks[i].accept_sock = -1;
    socks[i].overlap = NULL;
  }

  int listen_sock = create_listen_socket();
  if (listen_sock < 0) {
    return 0;
  }

  if (get_acceptex_ptr(listen_sock) != 0) {
    CLOSESOCK(listen_sock);
    display_msg("NETWORK ERROR: can't access AcceptEx function");
    return 0;
  }

  reconfig_event = WSACreateEvent();
  num_sockets = 0;

  while (1) {
    start_accept(listen_sock);
    DEBUG_MSG("-> created accept socked\n");
    
    int event_num = -1;
    int reconfig_requested = 0;
    while (1) {
      event_num = wait_event();
      if (event_num < 0) break;
      reconfig_requested = (event_num == num_sockets);
      handle_event(event_num);
      if (event_num == 0 || reconfig_requested) break;
    }

    if (event_num < 0) {
      display_msg("\r\nNETWORK ERROR");
      CLOSESOCK(listen_sock);
      return 0;
    }
    
    // reconfiguration event
    if (reconfig_requested) {
      // remove all events except the reconfiguration
      while (num_sockets > 1) {
        remove_socket(num_sockets-1);
      }

      // close and re-create listening socked
      CLOSESOCK(listen_sock);
      listen_sock = create_listen_socket();
      if (listen_sock < 0) {
        return 0;
      }
      continue;
    }
  }
  
  return 0;
}

int net_init(int port, fn_msg_proc msg_proc)
{
  msg_proc_func = msg_proc;
  server_port = port;
  
  DWORD net_thread_id;
  HANDLE net_thread = CreateThread(NULL, 0, net_main, NULL, 0, &net_thread_id);
  return (net_thread == NULL) ? 1 : 0;
}

void net_update_settings(int port)
{
  display_msg("\r\nReconfiguring...");
  server_port = port;
  WSASetEvent(reconfig_event);
}
