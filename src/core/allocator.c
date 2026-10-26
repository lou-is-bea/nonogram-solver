#include "nonogram/allocator.h"

#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* The only file of the project allowed to call the C library heap (ADR-0004). */

static bool is_power_of_two(size_t value)
{
    return value != 0u && (value & (value - 1u)) == 0u;
}

static void *heap_alloc([[maybe_unused]] void *ctx, size_t size, size_t align)
{
    /* No object may exceed PTRDIFF_MAX bytes (pointer differences must stay representable);
     * refusing such sizes here keeps them away from the C library heap altogether. */
    if (size == 0u || size > (size_t)PTRDIFF_MAX || !is_power_of_two(align)) {
        return nullptr;
    }
    if (align <= alignof(max_align_t)) {
        return malloc(size);
    }
    /* aligned_alloc historically requires a size that is a multiple of the alignment. */
    size_t rounded = 0;
    const bool overflow = ckd_add(&rounded, size, align - 1u);
    rounded -= rounded % align;
    if (overflow || rounded > (size_t)PTRDIFF_MAX) {
        return nullptr;
    }
    return aligned_alloc(align, rounded);
}

static void heap_free([[maybe_unused]] void *ctx, void *ptr, [[maybe_unused]] size_t size)
{
    free(ptr);
}

static const ng_allocator default_allocator = {
    .alloc = heap_alloc,
    .free = heap_free,
    .ctx = nullptr,
};

const ng_allocator *ng_allocator_default(void)
{
    return &default_allocator;
}
