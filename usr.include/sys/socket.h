#pragma once

#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

#include <chariot/net/in.h>
#include <chariot/net/socket.h>

#define __NEED_ssize_t
#define __NEED_size_t
#include <bits/alltypes.h>

int socket(int domain, int type, int protocol);

int accept(int sockfd, struct sockaddr *, int addrlen);
int connect(int sockfd, const struct sockaddr *addr, int addrlen);

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
	       const struct sockaddr *dest_addr, size_t addrlen);


int bind(int sockfd, struct sockaddr *addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif
