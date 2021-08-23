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
#define MAX_EVENTS       (2+MAX_CONNECTIONS)

struct SOCK_INFO {
  WSAOVERLAPPED *overlap;
  SOCKET sock;
  SOCKET accept_sock;
  char buf[1024];
  WSABUF wsa_buf;
};

static LPFN_ACCEPTEX pAcceptEx;

static fn_msg_proc msg_proc_func;
static int server_port;

static int num_events;
static WSAEVENT events[MAX_EVENTS];
static struct SOCK_INFO socks[MAX_EVENTS];
static char accept_buffer[2*sizeof(SOCKADDR_STORAGE) + 32];

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
  events[1] = WSACreateEvent();
  socks[1].sock = listen_sock;
  socks[1].accept_sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
  socks[1].overlap = XMALLOC(sizeof(WSAOVERLAPPED));
  memset(socks[1].overlap, 0, sizeof(WSAOVERLAPPED));
  socks[1].overlap->hEvent = events[1];
  if (events[1] == WSA_INVALID_EVENT) {
    DEBUG_MSG("ERROR in WSACreateEvent(): %d\n", WSAGetLastError());
  }
  if (num_events < 2) num_events = 2;

  DWORD bytes_read = 0;
  pAcceptEx(listen_sock,
            socks[1].accept_sock,
            accept_buffer,
            0,
            sizeof(SOCKADDR_STORAGE)+16,
            sizeof(SOCKADDR_STORAGE)+16,
            &bytes_read,
            socks[1].overlap
            );
}

static void remove_event(int event_num)
{
  CLOSESOCK(socks[event_num].sock);
  XFREE(socks[event_num].overlap);
  CLOSEEVENT(events[event_num]);
  
  for (int i = event_num; i < num_events-1; i++) {
    events[i] = events[i+1];
    socks[i] = socks[i+1];
  }
  num_events--;
}

static int wait_event(void)
{
  // Shuffle events (other than 0 and 1) to ensure
  // they all have a chance to be handled.
  if (num_events > 3) {
    WSAEVENT first_event = events[2];
    struct SOCK_INFO first_sock = socks[2];
    for (int i = 2; i < num_events-1; i++) {
      events[i] = events[i+1];
      socks[i] = socks[i+1];
    }
    events[num_events-1] = first_event;
    socks[num_events-1] = first_sock;
  }

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
  DEBUG_MSG("-> handling event %d\n", event_num);
  
  WSAResetEvent(events[event_num]);  // mark event processed
  if (event_num == 0) return;        // event 0 is reconfiguration
  struct SOCK_INFO *cur_sock = &socks[event_num];

  // read event result
  DWORD bytes = 0;
  DWORD flags = 0;
  int ret = WSAGetOverlappedResult(cur_sock->sock,
                                   cur_sock->overlap,
                                   &bytes,
                                   FALSE,
                                   &flags);
  if (ret == SOCKET_ERROR) {
    DEBUG_MSG("ERROR in WSAGetOverlappedResult() for event %d: %d\n", event_num, WSAGetLastError());
  }

  // connected socket closed
  if ((event_num != 1) && (bytes == 0)) {
    CLOSESOCK(cur_sock->accept_sock);
    XFREE(cur_sock->overlap);
    CLOSEEVENT(events[event_num]);
    remove_event(event_num);
    return;
  }

  // new connection accepted
  if (event_num == 1) {
    if (num_events >= MAX_EVENTS) { // no space for new connections
      CLOSESOCK(cur_sock->accept_sock);
      return;
    }

    // add event/socket
    int new_event_num = num_events++;
    events[new_event_num] = cur_sock->overlap->hEvent;
    struct SOCK_INFO *new_sock = &socks[new_event_num];
    new_sock->sock        = cur_sock->accept_sock;
    new_sock->overlap     = cur_sock->overlap;
    new_sock->wsa_buf.buf = new_sock->buf;
    new_sock->wsa_buf.len = sizeof(new_sock->buf);
    memset(new_sock->buf, 0, sizeof(new_sock->buf));

    // start receiving data
    flags = 0;
    int ret = WSARecv(new_sock->sock,
                      &new_sock->wsa_buf,
                      1,
                      &bytes,
                      &flags,
                      new_sock->overlap,
                      NULL
                      );
    if (ret == SOCKET_ERROR) {
      DWORD last_err = WSAGetLastError();
      if (last_err != WSA_IO_PENDING) {
        remove_event(new_event_num);
        return;
      }
    }
    return;
  }

  // data received
  if (bytes >= sizeof(cur_sock->buf)) {
    bytes = sizeof(cur_sock->buf)-1;
  }
  cur_sock->buf[bytes] = '\0';
  DEBUG_MSG("**** got data: '%.*s'\n", (int) bytes, cur_sock->buf);
  msg_proc_func(cur_sock->buf);

  // close connection
  remove_event(event_num);
}

static DWORD WINAPI net_main(LPVOID lpParam)
{
  display_msg("Starting...");
  
  WSADATA wsa_data;
  WSAStartup(MAKEWORD(2, 2), &wsa_data);

  for (int i = 0; i < MAX_EVENTS; i++) {
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
    return -1;
  }

  events[0] = WSACreateEvent();
  num_events = 1;

  while (1) {
    start_accept(listen_sock);
    DEBUG_MSG("-> created accept socked\n");
    
    int event_num = -1;
    while (1) {
      event_num = wait_event();
      if (event_num < 0) break;
      handle_event(event_num);
      if (event_num < 2) break;
    }

    if (event_num < 0) {
      display_msg("\r\nNETWORK ERROR: stopping");
      // TODO: shutdown
      continue;
    }
    
    // reconfiguration event
    if (event_num == 0) {
      // remove all events except the reconfiguration
      while (num_events > 1) {
        remove_event(num_events-1);
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
  WSASetEvent(events[0]);
}
