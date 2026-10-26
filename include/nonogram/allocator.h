/**
 * @file allocator.h
 * @brief Injectable allocator: every allocation of the library goes through one (ADR-0004).
 */
#ifndef NG_NONOGRAM_ALLOCATOR_H
#define NG_NONOGRAM_ALLOCATOR_H

#include <stddef.h>

/**
 * @brief Allocation interface passed explicitly to every function that allocates.
 *
 * Contract for implementations:
 * - alloc(ctx, size, align) returns a block of at least @p size bytes aligned on @p align,
 *   or nullptr when the request cannot be satisfied. The library only calls it with
 *   size > 0 and a power-of-two align.
 * - free(ctx, ptr, size) releases a block returned by alloc; @p size is the size passed to
 *   alloc, which allows exact accounting and arenas. free(ctx, nullptr, size) does nothing.
 * - Neither function may be nullptr. @p ctx is passed back unchanged and may be nullptr.
 *
 * A library object never outlives the allocator it was created with.
 */
typedef struct ng_allocator {
    void *(*alloc)(void *ctx, size_t size, size_t align);
    void (*free)(void *ctx, void *ptr, size_t size);
    void *ctx;
} ng_allocator;

/**
 * @brief Return the default allocator, which wraps the C library heap.
 *
 * It is stateless and safe to use from several threads at once. It returns nullptr when the
 * heap cannot satisfy the request, and for a zero size, a size above PTRDIFF_MAX or an align
 * that is not a power of two.
 *
 * @return A pointer to a static, immutable allocator. Never nullptr.
 * @ownership Owned by the library; valid for the whole program lifetime.
 * @complexity O(1) time and memory.
 */
const ng_allocator *ng_allocator_default(void);

#endif /* NG_NONOGRAM_ALLOCATOR_H */
