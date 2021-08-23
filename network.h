#ifndef NETWORK_H_FILE
#define NETWORK_H_FILE

typedef void (*fn_msg_proc)(const char *msg);

int net_init(int port, fn_msg_proc msg_proc);
void net_update_settings(int port);

#endif /* NETWORK_H_FILE */
