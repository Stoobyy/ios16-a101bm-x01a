/*
 * What qmi.c wants of net/sock.h: the type its connect() call casts the server's address to. The
 * QMI helpers under it (HakiOS's) take any address.
 */
#ifndef _LX10K_NET_SOCK_H
#define _LX10K_NET_SOCK_H

/* (not linux/socket.h: its kernel_connect() is not the QMI helpers' one) */

struct sockaddr_unsized;

#endif
