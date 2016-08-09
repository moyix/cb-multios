#include <libcgc.h>
#include "printf.h"
#include "cgc_libc.h"
#include "cgc_malloc.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#define MAX_BUCKETS    10

#define PAGE_SIZE 0x1000

#define PAGE_ALIGN(ptr) ((long int)(ptr) & -PAGE_SIZE);

/* BIT OPERATIONS */
#define BIT_SET(x, n)       ((x) |= (1ULL<<(n)))
#define BIT_UNSET(x, n)     ((x) &= ~(1ULL<<(n)))
#define BIT_IS_SET(x, n)    (((x) & (1ULL<<(n)))?1:0)

#define LIST_PREPEND(head,add)          \
    do {                                \
        assert((add) != NULL);          \
        (add)->prev = NULL;             \
        (add)->next = (head);           \
        if (head) {                     \
            (head)->prev = (add);       \
        }                               \
        (head) = (add);                 \
    } while (0)

#define LIST_REMOVE(head, del)                   \
    do {                                         \
        assert((head) != NULL);                  \
        assert((del) != NULL);                   \
        if ((del) == (head)) {                   \
            (head) = (del)->next;                \
        } else {                                 \
            if ((del)->prev) {                   \
                (del)->prev->next = (del)->next; \
            }                                    \
            if ((del)->next) {                   \
                (del)->next->prev = (del)->prev; \
            }                                    \
        }                                        \
    } while (0)

typedef struct _bucket cgc_bucket_t;
typedef struct chunk_header cgc_chunk_header_t;

struct chunk_header {
    size_t pages;
    size_t size;
    unsigned short elements_used;
    unsigned short elements_avail;
    unsigned int used_mask;
    cgc_bucket_t *bucket;
    cgc_chunk_header_t *next;
    cgc_chunk_header_t *prev;
};

struct _bucket {
    cgc_chunk_header_t *avail_list;
    cgc_chunk_header_t *full_list;
    unsigned short alloc_limit;
    unsigned short elements_per_page;
};

struct alloc_state {
    unsigned int pages;
    unsigned short largest_bucket;
    cgc_bucket_t buckets[MAX_BUCKETS];
    char initialised;
} __attribute__((__packed__));

static struct alloc_state state;

unsigned int cgc_ffs_u(unsigned int i);

static void cgc_malloc_init(void) {
    cgc_bucket_t *bucket = NULL;
    unsigned short alloc_limit = 128;
    unsigned short available;
    unsigned short i;
    state.pages = 0;
    cgc_memset(&state, 0, sizeof(state));
    available = PAGE_SIZE - sizeof(cgc_chunk_header_t);

    for (i = 0; i < MAX_BUCKETS; i++) {
        bucket = &state.buckets[i];
        bucket->alloc_limit = alloc_limit;
        bucket->elements_per_page = available / alloc_limit;
        assert(bucket->elements_per_page > 1);
        assert(bucket->elements_per_page <= 32);
        alloc_limit /= .75;
    }

    assert(bucket != NULL);
    assert(i == MAX_BUCKETS);
    state.largest_bucket = bucket->alloc_limit;
    assert(state.largest_bucket > 0);
    state.initialised = 1;
}

static void cgc_malloc_release_chunk(cgc_chunk_header_t *chunk) {
    int result;
    assert(chunk != NULL);
    assert(chunk->pages > 0);
    state.pages -= chunk->pages;
    result = deallocate(chunk, chunk->pages * PAGE_SIZE);
    assert(result == 0);
}

static cgc_chunk_header_t *cgc_malloc_request_chunk(size_t pages) {
    cgc_chunk_header_t *result;
    size_t size;
    int ret;

    size = pages * PAGE_SIZE;
    assert(size > 0);
    assert(size / pages == PAGE_SIZE);

    ret = allocate(size, 0, (void **)&result);
    assert(ret == 0);
    assert(result != NULL);
    result->pages = pages;
    state.pages += pages;
    return result;
}

static void *cgc_malloc_large(size_t size) {
    unsigned int pages;
    void *result;
    cgc_chunk_header_t *lh;
    pages = (sizeof(cgc_chunk_header_t) + size + (PAGE_SIZE - 1)) / PAGE_SIZE;
    lh = (cgc_chunk_header_t *) cgc_malloc_request_chunk(pages);
    lh->size = size;
    result = ((char *)lh) + sizeof(cgc_chunk_header_t);
    return result;
}

unsigned int cgc_ffs_u(unsigned int i) {
    unsigned int count = 0;

    if (i == 0) return 0;

    for (count = 0; count < 32; count++) {
        if (!BIT_IS_SET(i, count))
            break;
    }

    return count;
}

void *cgc_malloc(size_t size) {
    unsigned int i;
    void *ptr;
    cgc_bucket_t *bucket;
    cgc_chunk_header_t *chunk;
    assert(size > 0);

    if (state.initialised == 0)
        cgc_malloc_init();

    if (size > state.largest_bucket)
        return cgc_malloc_large(size);

    for (i = 0; i < MAX_BUCKETS; i++) {
        if (size <= state.buckets[i].alloc_limit)
            break;
    }

    bucket = &state.buckets[i];

    // assert(size <= bucket->alloc_limit);

    if (bucket->avail_list == NULL) {
        chunk = cgc_malloc_request_chunk(1);
        chunk->bucket = bucket;
        chunk->size   = bucket->alloc_limit;
        chunk->elements_avail = bucket->elements_per_page;
        LIST_PREPEND(bucket->avail_list, chunk);
    }

    chunk = bucket->avail_list;
    // assert(chunk->used_mask != 0xFFFFFFFF);
    i = cgc_ffs_u(chunk->used_mask);
    // assert(BIT_IS_SET(chunk->used_mask, i) == 0);
    chunk->elements_used++;
    BIT_SET(chunk->used_mask, i);

    if (chunk->elements_used == chunk->elements_avail) {
        LIST_REMOVE(bucket->avail_list, chunk);
        LIST_PREPEND(bucket->full_list, chunk);
        assert(bucket->full_list != NULL);
    }

    ptr = (void *)(((char *) chunk) + sizeof(cgc_chunk_header_t) + (bucket->alloc_limit * i));
    return ptr;
}

void cgc_free(void *ptr) {
    cgc_chunk_header_t *chunk;
    cgc_bucket_t *bucket;
    unsigned int i;
    assert(ptr != NULL);
    chunk = (cgc_chunk_header_t *) PAGE_ALIGN(ptr);

    if (chunk->bucket == NULL) {
        cgc_malloc_release_chunk(chunk);
        return;
    }

    bucket = chunk->bucket;
    i = (ptr - ((void *) chunk) - sizeof(cgc_chunk_header_t)) / bucket->alloc_limit;
    assert(BIT_IS_SET(chunk->used_mask, i));
    BIT_UNSET(chunk->used_mask, i);

    if (chunk->elements_used == chunk->elements_avail) {
        LIST_REMOVE(bucket->full_list, chunk);
        LIST_PREPEND(bucket->avail_list, chunk);
    }

    chunk->elements_used--;

    if (chunk->elements_used == 0) {
        LIST_REMOVE(bucket->avail_list, chunk);
        cgc_malloc_release_chunk(chunk);
    }
}

#define MUL_NO_OVERFLOW (1UL << (sizeof(size_t) * 4))

void * cgc_calloc(size_t nmemb, size_t size) {
    void *ptr;

    assert(nmemb > 0);
    assert(size > 0);
    assert(size < ( (unsigned int) SIZE_MAX / nmemb));

    size *= nmemb;
    ptr = cgc_malloc(size);
    return cgc_memset(ptr, 0, size);
}
