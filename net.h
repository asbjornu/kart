#ifndef NET_H
#define NET_H

#define NET_INPUT_UP 1<<0
#define NET_INPUT_DOWN 1<<1
#define NET_INPUT_LEFT 1<<2
#define NET_INPUT_RIGHT 1<<3
#define NET_INPUT_SPACE 1<<4
#define NET_DELIM ":"

int net_init();
void net_cleanup();
int net_start_server(int port);
int net_start_client(const char *addr, int port);
int net_accept();
int net_recv(int socket, char *buf, int buf_len);
void net_set_input(unsigned input);
void net_send_input(int sockfd, unsigned long long tic);
void net_close(int socket);

#endif /*NET_H*/
