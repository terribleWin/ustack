/*
 * RFC 826 - An Ethernet Address Resolution Protocol
 * 
 * Actually, the real ARP header is only the first 8 bytes. 
 * The rest 20 bytes are called ARP packet or ARP payload according to RFC 826.
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Hardware Type         |         Protocol Type         |
 * |           (ar$hrd)            |           (ar$pro)            |
 * |           (2 Bytes)           |           (2 Bytes)           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |     HLEN      |     PLEN      |         Operation Code        |
 * |   (ar$hln)    |   (ar$pln)    |           (ar$op)             |
 * |   (1 Byte)    |   (1 Byte)    |           (2 Bytes)           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                     Sender Hardware Address                   +
 * |                         (ar$sha, 6 Bytes)                     |
 * +                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                               |                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 * |                                                               |
 * +                     Sender Protocol Address                   +
 * |                         (ar$spa, 4 Bytes)                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                     Target Hardware Address                   +
 * |                         (ar$tha, 6 Bytes)                     |
 * +                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                               |                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 * |                                                               |
 * +                     Target Protocol Address                   +
 * |                         (ar$tpa, 4 Bytes)                     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 * Only the Ethernet + IPv4 ARP is handled here, that is ar$hrd == 1 and
 * ar$pro == 0x0800. A request whose target IP is the local IP is answered,
 * a reply is only used to learn the MAC address of the peer.
 * 
 * Field definitions:
 *  16.bit (ar$hrd) : Hardware address space. 1 = Ethernet.
 *  16.bit (ar$pro) : Protocol address space. 0x0800 = IPv4.
 *  8.bit  (ar$hln) : Byte length of each hardware address. 6 for Ethernet.
 *  8.bit  (ar$pln) : Byte length of each protocol address. 4 for IPv4.
 *  16.bit (ar$op)  : Opcode. 1 = request, 2 = reply.
 *  48.bit (ar$sha) : Hardware address of the sender of this packet.
 *  32.bit (ar$spa) : Protocol address of the sender of this packet.
 *  48.bit (ar$tha) : Hardware address of the target of this packet.
 *  32.bit (ar$tpa) : Protocol address of the target.
 */
#ifndef __NG_ARP_H__
#define __NG_ARP_H__

#include <pthread.h>
#include <string.h>

#include <rte_ether.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_debug.h>

#define ARP_ENTRY_STATUS_DYNAMIC 0
#define ARP_ENTRY_STATUS_STATIC 1

#define LL_ADD(item, list) do { \
    item->prev = NULL;          \
    item->next = list;          \
    if (list !=NULL) list->prev = item; \
    list = item;                \
} while(0)

#define LL_REMOVE(item, list) do {      \
    if (item->prev != NULL) item->prev->next = item->next; \
    if (item->next != NULL) item->next->prev = item->prev; \
    if (list == item) list = item->next;	\
	item->prev = item->next = NULL;			\
} while(0)

struct arp_entry{
    uint32_t ip;
    uint8_t hwaddr[RTE_ETHER_ADDR_LEN];

    uint8_t type;

	uint64_t last_update_time;
	uint8_t retry_count;                // 当前重传次数 (用于主动 ARP 请求)

    struct arp_entry *next;
    struct arp_entry *prev;
};

struct arp_table {

    struct arp_entry *entries;
    int count; // table 里面的条目数

    pthread_spinlock_t spinlock;

};

// 获取当前时间戳，单位毫秒
static uint64_t get_current_time_ms(void) {
    return rte_get_timer_cycles() / (rte_get_timer_hz() / 1000);
}

// arp 表全局只有唯一一张表
static struct arp_table *arpt = NULL;

/// @brief 获取 arp 表的唯一实例
/// @return arp 表实例指针
static struct  arp_table *arp_table_instance(void) {

	if (arpt == NULL) {

		arpt = rte_malloc("arp table", sizeof(struct  arp_table), 0);
		if (arpt == NULL) {
			rte_exit(EXIT_FAILURE, "rte_malloc arp table failed\n");
		}
		memset(arpt, 0, sizeof(struct  arp_table));
		pthread_spin_init(&arpt->spinlock, 0);
	}

	return arpt;

}

/// @brief search ARP item by IP
/// @param ip targeted IP
/// @return return the hit entry, else return NULL
static struct arp_entry *ng_arp_entry_lookup(uint32_t ip) {

    struct arp_table *table = arp_table_instance();
	struct arp_entry *iter;
    for (iter = table->entries; iter != NULL; iter = iter->next) {
        if (iter->ip == ip) {
            return iter;
        }
    }

    return NULL;
}

/// @brief 查找 arp 表中是否存在指定 IP 的条目
/// @param ip 要查找的 IP 地址（网络字节序）
/// @return 如果存在，返回 mac 地址指针；否则返回 NULL
static uint8_t* ng_get_dst_macaddr(uint32_t dip) {

	struct arp_entry *iter;
	struct arp_table *table = arp_table_instance();

	// TODO[FIXME]: 采用ng_arp_entry_lookup实现
	// iter = ng_arp_entry_lookup(dip);
	// return iter->hwaddr;
	for (iter = table->entries;iter != NULL;iter = iter->next) {
		if (dip == iter->ip) {
			return iter->hwaddr;
		}
	}

	return NULL;
}

/// @brief This function is used to insert a arp item into the table
/// @param ip The source IP 
/// @param mac The source MAC address
/// @return 1 if a new item is inserted, 0 if the item already exists and its ageing time is refreshed,
///         -1 if the memory allocation failed
static int ng_arp_entry_insert(uint32_t ip, const uint8_t *mac) {

	struct arp_table *table = arp_table_instance();
	uint64_t now = get_current_time_ms();
	struct arp_entry *entry = ng_arp_entry_lookup(ip); // search the arp item

	// If this item exists, refresh the ageing time, and overwrite the mac when it changed
	if (entry != NULL) {
		pthread_spin_lock(&table->spinlock);

		if (memcmp(entry->hwaddr, mac, RTE_ETHER_ADDR_LEN) != 0) {
			rte_memcpy(entry->hwaddr, mac, RTE_ETHER_ADDR_LEN);
		}

		entry->last_update_time = now;
		entry->retry_count = 0;

		pthread_spin_unlock(&table->spinlock);

		return 0; // already exists, only refresh the ageing time
	}

	// If this item does not exist, insert it into the table
	entry = rte_malloc("arp_entry", sizeof(struct arp_entry), 0);
	if (entry == NULL) return -1;

	memset(entry, 0, sizeof(struct arp_entry));

	entry->ip = ip;
	rte_memcpy(entry->hwaddr, mac, RTE_ETHER_ADDR_LEN);
	entry->type = ARP_ENTRY_STATUS_DYNAMIC;
	entry->last_update_time = now;


	pthread_spin_lock(&table->spinlock);
	LL_ADD(entry, table->entries);
	table->count ++;
	pthread_spin_unlock(&table->spinlock);
			
	return 1; // insert successfully
	
}


#endif