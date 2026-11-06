

#include "epoll.h"
#include "fd.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <rte_malloc.h>
#include <rte_common.h>


/*
 * 红黑树比较函数：按 sockfd 排序
 * nty_tree.h 会据此生成 RB_INSERT/REMOVE/FIND 等静态函数
 */
static int sockfd_cmp(struct epitem *ep1, struct epitem *ep2) {
	if (ep1->sockfd < ep2->sockfd) return -1;
	else if (ep1->sockfd == ep2->sockfd) return 0;
	return 1;
}

RB_GENERATE_STATIC(_epoll_rb_socket, epitem, rbn, sockfd_cmp);


/*
 * 当前实现只支持一个全局 eventpoll 实例（与 tcp_concurrency.c 一致）。
 * 如果需要支持多个 epoll 实例，应换成以 epfd 为 key 的表/树。
 */
static struct eventpoll *g_eventpoll = NULL;

struct eventpoll *ng_epoll_instance(void) {
	return g_eventpoll;
}


/*
 * 协议栈内部在事件就绪时调用此函数，把 sockfd 挂到就绪链表并唤醒 wait。
 * 调用点（tcp.c 中需要补）：
 * 1. listen fd 上有新连接完成三次握手时 -> EPOLLIN
 * 2. established stream 收到 PSH 数据入 rcvbuf 后 -> EPOLLIN
 * 3. established stream 收到 FIN（nrecv 将返回 0）时 -> EPOLLIN
 */
int epoll_event_callback(struct eventpoll *ep, int sockfd, uint32_t event) {

	// TODO: 构造临时 epitem，用 RB_FIND 在 ep->rbr 中按 sockfd 查找

	// TODO: 若找不到，打印警告并返回 -1

	// TODO: 若 epi->rdy 已为 1，说明已在就绪链表，直接把 event 按位或到 epi->event.events，返回 1

	// TODO: 否则：spin_lock(&ep->lock)，LIST_INSERT_HEAD(&ep->rdlist, epi, rdlink)，epi->rdy = 1，ep->rdnum++，spin_unlock

	// TODO: 若 ep->waiting 为真，用 pthread_cond_signal 唤醒阻塞在 nepoll_wait 的线程

	return 0;
}


/*
 * 对应 Linux epoll_create：分配一个 fd 和一个 eventpoll 结构。
 * 注意：tcp.c 中的 get_fd_frombitmap/set_fd_frombitmap 需要去掉 static。
 */
int nepoll_create(int size) {

	// TODO: size <= 0 返回 -1

	// TODO: 用 get_fd_frombitmap() 分配 epoll 自身的 fd

	// TODO: 用 rte_malloc 分配 struct eventpoll 并 memset 为 0

	// TODO: 初始化 rbr（RB_INIT）、rdlist（LIST_INIT）、各类锁/cond

	// TODO: 保存 ep->fd，设置 g_eventpoll = ep

	return -1;
}


/*
 * 对应 Linux epoll_ctl：把 sockfd 加入/修改/移除 epoll 监听集合。
 */
int nepoll_ctl(int epfd, int op, int sockfd, struct epoll_event *event) {

	// TODO: 通过 get_hostinfo_fromfd(epfd) 拿到 struct eventpoll*（或直接用 g_eventpoll 校验 epfd）
	//       非法时设置 errno = EINVAL 并返回 -1

	// TODO: EPOLL_CTL_ADD:
	//       - 用 RB_FIND 检查是否已存在，存在返回 -1
	//       - rte_malloc 分配 epitem，填入 sockfd 和 event->events/data
	//       - RB_INSERT 到 rbr，rbcnt++

	// TODO: EPOLL_CTL_DEL:
	//       - RB_FIND 找到 epitem
	//       - 若 epi->rdy == 1，从 rdlist 摘除，rdnum--
	//       - RB_REMOVE，rbcnt--，rte_free(epitem)

	// TODO: EPOLL_CTL_MOD:
	//       - RB_FIND 找到 epitem
	//       - 更新 epi->event.events 和 data
	//       - 通常不需要动 rdlist；如果已经在就绪链表，可以 OR 新事件

	return -1;
}


/*
 * 对应 Linux epoll_wait：阻塞/超时等待就绪事件。
 */
int nepoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout) {

	// TODO: 校验 epfd 和 maxevents

	// TODO: 加 ep->lock 自旋锁，检查 rdnum
	//       - 若 rdnum > 0：从 rdlist 头部依次弹出最多 maxevents 个 epitem，
	//         把 epi->event 复制到 events[]，清除 epi->rdy，rdnum--，解锁，返回事件数

	// TODO: 若无事件：
	//       - timeout == 0 直接返回 0
	//       - timeout > 0 或 -1：pthread_mutex_lock(&ep->cdmtx)，ep->waiting = 1，
	//         按 timeout 调用 pthread_cond_timedwait 或 pthread_cond_wait

	// TODO: 被唤醒后再次检查 rdlist并取事件；取完后 ep->waiting = 0，解锁 cdmtx

	return -1;
}
