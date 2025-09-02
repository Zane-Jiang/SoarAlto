#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mbmalloc.h"

int main() {
    size_t size = 400 * 1024 * 1024; 
    long pgsz = sysconf(_SC_PAGESIZE);
    size_t pages = (size + pgsz - 1) / pgsz;

    uint8_t* bitmap = calloc(pages, 1);
    if (!bitmap) { perror("calloc"); return 1; }

    for (size_t i = pages/2; i < pages; ++i) bitmap[i] = 1;

    int remote_nid = 2; 
    void* p = malloc_bitmap(size, bitmap, pages, remote_nid, 1);
    if (!p) { perror("malloc_bitmap"); free(bitmap); return 2; }

    int* nodes = malloc(pages * sizeof(int));
    if (!nodes) { perror("malloc"); free(bitmap); return 3; }

    if (query_pages_nodes(p, size, nodes) == 0) {
        printf("page index : node\n");
        for (size_t i = 0; i < pages; ++i)
            printf("%6zu : %d\n", i, nodes[i]);
    } else {
        perror("query_pages_nodes");
    }

    memset(p, 0xAB, size); 

    free(nodes);
    free(bitmap);
    free_bitmap(p, size);
    return 0;
}
