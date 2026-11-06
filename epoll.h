

#ifndef __NG_EPOLL_H__
#define __NG_EPOLL_H__

#include <stdint.h>
#include <pthread.h>
#include <sys/queue.h>
#include "nty_tree.h"


enum EPOLL_EVENTS {
	EPOLLNONE 	= 0x0000,
	EPOLLIN 	= 0x0001,
	EPOLLPRI	= 0x0002,
	EPOLLOUT	= 0x0004,
	EPOLLRDNORM = 0x0040,
	EPOLLRDBAND = 0x0080,
	EPOLLWRNORM = 0x0100,
	EPOLLWRBAND = 0x0200,
	EPOLLMSG	= 0x0400,
	EPOLLERR 	= 0x0008,
	EPOLLHUP 	= 0x0010,
	EPOLLRDHUP 	= 0x2000,
	EPOLLONESHOT = (1 << 30),
	EPOLLET 	= (1 << 31)
};

#define EPOLL_CTL_ADD	1
#define EPOLL_CTL_DEL	2
#define EPOLL_CTL_MOD	3

typedef union epoll_data {
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;

struct epoll_event {
	uint32_t events;
	epoll_data_t data;
};

/*
 * 每个被监听的 socket 对应一个 epitem：
 * - rbn: 红黑树节点，用于 O(log n) 查找
 * - rdlink: 就绪链表节点
 * - rdy: 是否已经在就绪链表中
 */
struct epitem {
	RB_ENTRY(epitem) rbn;
	LIST_ENTRY(epitem) rdlink;
	int rdy;

	int sockfd;
	struct epoll_event event;
};

RB_HEAD(_epoll_rb_socket, epitem);
typedef struct _epoll_rb_socket ep_rb_tree;

/*
 * 一个 epoll 实例：
 * - rbr: 以 sockfd 为 key 的红黑树，保存所有注册项，本质上是红黑树根节点
 * - rdlist: 就绪链表， Nepoll_wait 从这里取事件
 * - mtx: 保护红黑树的互斥锁
 * - lock: 保护就绪链表的自旋锁
 * - cond/cdmtx: 让 nepoll_wait 阻塞等待事件
 */
struct eventpoll {
	ep_rb_tree rbr;
	int rbcnt;

	LIST_HEAD( ,epitem) rdlist;
	int rdnum;

	int waiting;

	pthread_mutex_t mtx; //rbtree update
	pthread_spinlock_t lock; //rdlist update
	
	pthread_cond_t cond; //block for event
	pthread_mutex_t cdmtx; //mutex for cond

	int fd; // epoll 实例自身的 fd
};

/* 返回当前唯一的 eventpoll 实例；未创建时返回 NULL */
struct eventpoll *ng_epoll_instance(void);

int epoll_event_callback(struct eventpoll *ep, int sockfd, uint32_t event);
int nepoll_create(int size);
int nepoll_ctl(int epfd, int op, int sockfd, struct epoll_event *event);
int nepoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

#endif
