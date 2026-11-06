

#ifndef __NG_FD_H__
#define __NG_FD_H__

#include <fcntl.h>
#include <sys/socket.h>
/*
 * fd 管理相关函数，定义在 tcp.c 中，但 epoll.c 需要调用，
 * 所以通过头文件暴露出来（实现中需去掉 static 修饰）。
 */

int get_fd_frombitmap(void);
int set_fd_frombitmap(int fd);
void *get_hostinfo_fromfd(int sockfd);
struct ustack_udp_sock * get_hostinfo_fromip_port(uint32_t dip, uint16_t port, uint8_t proto);

#endif
