#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/allocator.h"

#include "ng_test_fixture.h"

TestSuite(allocator_default, .init = ng_test_setup, .fini = ng_test_teardown);

static bool is_aligned(const void *ptr, size_t align)
{
    return ((uintptr_t)ptr % align) == 0u;
}

Test(allocator_default, returns_the_same_complete_allocator)
{
    const ng_allocator *first = ng_allocator_default();

    const ng_allocator *second = ng_allocator_default();

    cr_assert_not_null(first);
    cr_assert_eq(first, second);
    cr_assert_not_null(first->alloc);
    cr_assert_not_null(first->free);
}

Test(allocator_default, block_is_writable_over_its_whole_size)
{
    const ng_allocator *a = ng_allocator_default();
    constexpr size_t size = 1'000;

    unsigned char *block = a->alloc(a->ctx, size, 1);

    cr_assert_not_null(block);
    memset(block, 0xA5, size);
    cr_assert_eq(block[size - 1u], 0xA5);
    a->free(a->ctx, block, size);
}

Test(allocator_default, honours_every_power_of_two_alignment)
{
    const ng_allocator *a = ng_allocator_default();
    const size_t aligns[] = {1, 2, 8, alignof(max_align_t), 64, 4'096};

    for (size_t i = 0; i < sizeof aligns / sizeof aligns[0]; ++i) {
        void *block = a->alloc(a->ctx, 3, aligns[i]);

        cr_assert_not_null(block, "align %zu", aligns[i]);
        cr_assert(is_aligned(block, aligns[i]), "align %zu", aligns[i]);
        a->free(a->ctx, block, 3);
    }
}

Test(allocator_default, zero_size_returns_nullptr)
{
    const ng_allocator *a = ng_allocator_default();

    void *block = a->alloc(a->ctx, 0, 1);

    cr_assert_null(block);
}

Test(allocator_default, non_power_of_two_alignment_returns_nullptr)
{
    const ng_allocator *a = ng_allocator_default();
    const size_t aligns[] = {0, 3, 24, SIZE_MAX};

    for (size_t i = 0; i < sizeof aligns / sizeof aligns[0]; ++i) {
        cr_assert_null(a->alloc(a->ctx, 16, aligns[i]), "align %zu", aligns[i]);
    }
}

Test(allocator_default, size_above_ptrdiff_max_returns_nullptr)
{
    const ng_allocator *a = ng_allocator_default();
    const size_t too_big = (size_t)PTRDIFF_MAX + 1u;

    cr_assert_null(a->alloc(a->ctx, too_big, 1));
    cr_assert_null(a->alloc(a->ctx, SIZE_MAX, 1));
    cr_assert_null(a->alloc(a->ctx, SIZE_MAX - 8u, 64));
}

Test(allocator_default, size_overflowing_alignment_rounding_returns_nullptr)
{
    const ng_allocator *a = ng_allocator_default();

    /* passes the PTRDIFF_MAX check only if it were applied after rounding */
    cr_assert_null(a->alloc(a->ctx, (size_t)PTRDIFF_MAX, 4'096));
}

Test(allocator_default, largest_power_of_two_alignment_returns_nullptr)
{
    const ng_allocator *a = ng_allocator_default();
    const size_t huge_align = (SIZE_MAX / 2u) + 1u;

    cr_assert_null(a->alloc(a->ctx, 1, huge_align));
}

Test(allocator_default, free_of_nullptr_does_nothing)
{
    const ng_allocator *a = ng_allocator_default();

    a->free(a->ctx, nullptr, 16);
}
