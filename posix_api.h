/*
 * POSIX.1-2017 - IEEE Std 1003.1-2017
 * System Interfaces: Sockets
 *
 * This module provides the standard BSD/POSIX socket API over the
 * user-space DPDK TCP/IP stack. Applications can create sockets,
 * bind them to addresses, listen, accept, connect, send, and receive
 * data without modifying existing application code.
 *
 * Supported calls (initial subset):
 *   - socket()    : create a new socket (AF_INET/AF_INET6, SOCK_STREAM, etc.).
 *   - bind()      : bind to a local address and port.
 *   - listen()    : mark a socket as passive.
 *   - accept()    : accept an incoming connection.
 *   - connect()   : initiate a connection.
 *   - shutdown()  : half-close a socket.
 *   - send()/sendto()/sendmsg() : transmit data.
 *   - recv()/recvfrom()/recvmsg() : receive data.
 *   - close()     : close a socket and release resources.
 *   - setsockopt()/getsockopt() : socket and TCP options.
 *   - select()/poll()/epoll_wait() : I/O multiplexing (see epoll.h).
 *
 * File descriptors are managed by the fd.h layer so they integrate
 * with the application's event loop. Each socket wraps a stack
 * connection or listening endpoint.
 */
#ifndef __NG_POSIX_API_H__
#define __NG_POSIX_API_H__

#include <sys/types.h>
#include <sys/socket.h>

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen);

int close(int fd);

#endif
