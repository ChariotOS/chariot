#pragma once


#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <chariot/net/socket.h>
#include <chariot/net/in.h>

int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr *addr, int addrlen);

#ifdef __cplusplus
}
#endif

#endif
