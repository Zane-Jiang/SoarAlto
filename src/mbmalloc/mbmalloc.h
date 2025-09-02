#ifndef MBMALLOC_H
#define MBMALLOC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* malloc_bitmap:
 *  分配 size 字节（按页上取整）的匿名内存，
 *  bitmap: 每页一个字节 (0/1)；bitmap_len 表示 bitmap 的长度（页数），
 *  bitmap[i]==0 表示该页放本地节点（自动猜测），==1 表示放 remote_nid。
 *  remote_nid: 远端节点 id（例如 CXL 的 NUMA node）
 *  prefault: 是否立即触发触页（1=触页，0=懒触发）
 *
 *  返回映射的地址（已可读写），失败返回 NULL 且设置 errno。
 */
void* malloc_bitmap(size_t size,
                    const uint8_t* bitmap,
                    size_t bitmap_len,
                    int remote_nid,
                    int prefault);

/* free_bitmap: 释放 malloc_bitmap 分配的内存（size 必须是原始 size） */
void free_bitmap(void* p, size_t size);

/* query_pages_nodes: 查询指定地址区间每页当前所在的 NUMA 节点。
 * nodes 数组长度应至少为 (len + pagesize-1)/pagesize
 * 返回 0 成功，-1 失败（errno 会被设置）
 */
int query_pages_nodes(void* addr, size_t len, int* nodes);

#ifdef __cplusplus
}
#endif

#endif /* MBMALLOC_H */
