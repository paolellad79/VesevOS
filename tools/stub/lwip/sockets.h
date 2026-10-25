#pragma once
// STUB di lwip/sockets.h (solo quello che usano vos_drv_udp.cpp e vos_drv_tcpsrv.cpp)
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
int lwip_socket(int, int, int);
int lwip_bind(int, const struct sockaddr*, socklen_t);
int lwip_close(int);
int lwip_setsockopt(int, int, int, const void*, socklen_t);
int lwip_recvfrom(int, void*, size_t, int, struct sockaddr*, socklen_t*);
int lwip_sendto(int, const void*, size_t, int, const struct sockaddr*, socklen_t);
int lwip_listen(int, int);
int lwip_accept(int, struct sockaddr*, socklen_t*);
int lwip_recv(int, void*, size_t, int);
int lwip_send(int, const void*, size_t, int);
int lwip_shutdown(int, int);
