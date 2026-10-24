#include "test_allocator.h"

#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"

/* Stored immediately before each user block. */
typedef struct block_header {
    size_t size;
    size_t offset; /* from the start of the underlying block to the user block */
} block_header;

static bool is_power_of_two(size_t value)
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

/* Offset of the user block: a multiple of its alignment that leaves room for the header. */
static size_t header_offset(size_t align)
{
    const size_t header_align = alignof(block_header);
    const size_t base = align > header_align ? align : header_align;
    size_t offset = base;
    while (offset < sizeof(block_header)) {
        offset += base;
    }
    return offset;
}

static block_header *header_of(void *ptr)
{
    return (block_header *)((unsigned char *)ptr - sizeof(block_header));
}

static void *test_alloc(void *ctx, size_t size, size_t align)
{
    ng_test_allocator *ta = ctx;
    const ng_counter request = ta->request_count++;
    if (size == 0u || !is_power_of_two(align)) {
        ++ta->misuse_count;
        return nullptr;
    }
    if (request == ta->fail_at) {
        return nullptr;
    }
    size_t live_after = 0;
    if (ckd_add(&live_after, ta->live_bytes, size) || live_after > ta->cap_bytes) {
        return nullptr;
    }
    const size_t offset = header_offset(align);
    size_t total = 0;
    if (ckd_add(&total, offset, size)) {
        return nullptr;
    }
    const size_t underlying_align = align > alignof(block_header) ? align : alignof(block_header);
    const ng_allocator *heap = ng_allocator_default();
    unsigned char *raw = heap->alloc(heap->ctx, total, underlying_align);
    if (raw == nullptr) {
        return nullptr;
    }
    unsigned char *user = raw + offset;
    *header_of(user) = (block_header){.size = size, .offset = offset};

    ta->live_bytes = live_after;
    if (live_after > ta->peak_bytes) {
        ta->peak_bytes = live_after;
    }
    ++ta->alloc_count;
    return user;
}

static void test_free(void *ctx, void *ptr, size_t size)
{
    ng_test_allocator *ta = ctx;
    if (ptr == nullptr) {
        return;
    }
    const block_header header = *header_of(ptr);
    if (header.size != size) {
        ++ta->misuse_count;
    }
    ta->live_bytes -= header.size;
    ++ta->free_count;
    const ng_allocator *heap = ng_allocator_default();
    heap->free(heap->ctx, (unsigned char *)ptr - header.offset, header.offset + header.size);
}

void ng_test_allocator_init(ng_test_allocator *ta)
{
    *ta = (ng_test_allocator){
        .base = {.alloc = test_alloc, .free = test_free, .ctx = ta},
        .fail_at = NG_TEST_NEVER_FAIL,
        .cap_bytes = NG_TEST_NO_CAP,
    };
}

void ng_test_allocator_fail_at(ng_test_allocator *ta, ng_counter n)
{
    ta->request_count = 0;
    ta->fail_at = n;
}

void ng_test_allocator_cap(ng_test_allocator *ta, size_t cap_bytes)
{
    ta->cap_bytes = cap_bytes;
}
