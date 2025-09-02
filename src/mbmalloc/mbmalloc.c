#define _GNU_SOURCE
#include "mbmalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/mempolicy.h>
#include <numaif.h>
#include <stdint.h>
#include <assert.h>

#ifndef MPOL_MF_STRICT
#define MPOL_MF_STRICT 0x1
#endif
#ifndef MPOL_MF_MOVE
#define MPOL_MF_MOVE   0x2
#endif
#ifndef MPOL_MF_MOVE_ALL
#define MPOL_MF_MOVE_ALL 0x4
#endif

static long get_page_size(void) {
    static long pgsz = 0;
    if (!pgsz) {
        pgsz = sysconf(_SC_PAGESIZE);
        if (pgsz <= 0) pgsz = 4096;
    }
    return pgsz;
}

static int guess_local_nid(void) {
    unsigned long nodemask_arr[16];
    memset(nodemask_arr, 0, sizeof(nodemask_arr));
    int mode = 0;
    if (get_mempolicy(&mode, nodemask_arr, sizeof(nodemask_arr) * 8, NULL, 0) == 0 &&
        mode == MPOL_PREFERRED) {
        return (int)nodemask_arr[0];
    }
    return 0; /* fallback */
}

static int mbind_bind_single_node(void* addr, size_t len, int nid, unsigned flags) {
    const int maxnodes = 1024;
    size_t nlongs = (maxnodes + (8 * sizeof(unsigned long) - 1)) / (8 * sizeof(unsigned long));
    unsigned long* nodemask = calloc(nlongs, sizeof(unsigned long));
    if (!nodemask) { errno = ENOMEM; return -1; }
    size_t idx = nid / (8 * sizeof(unsigned long));
    size_t bit = nid % (8 * sizeof(unsigned long));
    nodemask[idx] = (1UL << bit);

    long rc = syscall(SYS_mbind, addr, len, MPOL_BIND, nodemask, maxnodes, flags);
    int saved_errno = errno;
    free(nodemask);
    if (rc == -1) {
        errno = saved_errno;
        return -1;
    }
    return 0;
}

static void prefault_touch_pages(void* addr, size_t len) {
    long pgsz = get_page_size();
    volatile char* p = (volatile char*)addr;
    for (size_t off = 0; off < len; off += pgsz) {
        char old = p[off];
        p[off] = old;
    }
}

int query_pages_nodes(void* addr, size_t len, int* nodes) {
    if (!addr || !nodes) { errno = EINVAL; return -1; }
    long pgsz = get_page_size();
    size_t pages = (len + pgsz - 1) / pgsz;

    void** addrs = malloc(pages * sizeof(void*));
    int* status = malloc(pages * sizeof(int));
    if (!addrs || !status) { free(addrs); free(status); errno = ENOMEM; return -1; }

    for (size_t i = 0; i < pages; ++i) addrs[i] = (char*)addr + i * pgsz;

    long rc = move_pages(0, (int)pages, addrs, NULL, status, 0);
    int saved_errno = errno;
    if (rc == -1) { free(addrs); free(status); errno = saved_errno; return -1; }

    for (size_t i = 0; i < pages; ++i) nodes[i] = status[i];
    free(addrs);
    free(status);
    return 0;
}

static int apply_bitmap_policy(void* base, size_t size,
                               const uint8_t* bitmap, size_t bitmap_len,
                               int local_nid, int remote_nid) {
    long pgsz = get_page_size();
    size_t pages = (size + pgsz - 1) / pgsz;
    if (bitmap_len < pages) { errno = EINVAL; return -1; }

    size_t i = 0;
    while (i < pages) {
        uint8_t bit = bitmap[i] ? 1 : 0;
        size_t j = i + 1;
        while (j < pages && (bitmap[j] ? 1 : 0) == bit) ++j;

        void* run_addr = (char*)base + i * pgsz;
        size_t run_len = (j - i) * pgsz;
        int nid = bit ? remote_nid : local_nid;
        if (mbind_bind_single_node(run_addr, run_len, nid, 0) != 0) return -1;
        i = j;
    }
    return 0;
}

void* malloc_bitmap(size_t size,
                    const uint8_t* bitmap,
                    size_t bitmap_len,
                    int remote_nid,
                    int prefault) {
    if (size == 0) return NULL;
    long pgsz = get_page_size();
    size_t rounded = ((size + pgsz - 1) / pgsz) * pgsz;

    int local_nid = guess_local_nid();

    void* addr = mmap(NULL, rounded, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED) return NULL;

    madvise(addr, rounded, MADV_NOHUGEPAGE);

    if (apply_bitmap_policy(addr, rounded, bitmap, bitmap_len, local_nid, remote_nid) != 0) {
        int saved_errno = errno;
        munmap(addr, rounded);
        errno = saved_errno;
        return NULL;
    }

    if (prefault) prefault_touch_pages(addr, rounded);

    return addr;
}

void free_bitmap(void* p, size_t size) {
    if (!p || size == 0) return;
    long pgsz = get_page_size();
    size_t rounded = ((size + pgsz - 1) / pgsz) * pgsz;
    munmap(p, rounded);
}
