#ifndef _NET_UDP
#define _NET_UDP

typedef struct _socket_udp socket_udp;

socket_udp *udp_init(char *addr, int port, int ttl);
int         udp_send(socket_udp *s, char *buffer, int buflen);
int         udp_recv(socket_udp *s, char *buffer, int buflen);

#endif

