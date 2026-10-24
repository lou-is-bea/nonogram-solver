#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/allocator.h"

#include "ng_test_fixture.h"
#include "test_allocator.h"

/* The instrumented allocator is the basis of every leak and allocation-failure test: it is
 * tested like library code. */
TestSuite(support_allocator, .init = ng_test_setup, .fini = ng_test_teardown);

static void *request(ng_test_allocator *ta, size_t size, size_t align)
{
    return ta->base.alloc(ta->base.ctx, size, align);
}

static void release(ng_test_allocator *ta, void *ptr, size_t size)
{
    ta->base.free(ta->base.ctx, ptr, size);
}

Test(support_allocator, init_starts_empty_and_unlimited)
{
    ng_test_allocator ta;

    ng_test_allocator_init(&ta);

    cr_assert_eq(ta.live_bytes, 0u);
    cr_assert_eq(ta.peak_bytes, 0u);
    cr_assert_eq(ta.alloc_count, 0u);
    cr_assert_eq(ta.misuse_count, 0u);
    cr_assert_eq(ta.fail_at, NG_TEST_NEVER_FAIL);
    cr_assert_eq(ta.cap_bytes, NG_TEST_NO_CAP);
}

Test(support_allocator, counts_live_and_peak_bytes)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);

    void *a = request(&ta, 100, 1);
    void *b = request(&ta, 50, 8);
    release(&ta, a, 100);

    cr_assert_eq(ta.live_bytes, 50u);
    cr_assert_eq(ta.peak_bytes, 150u);
    cr_assert_eq(ta.alloc_count, 2u);
    cr_assert_eq(ta.free_count, 1u);
    release(&ta, b, 50);
    cr_assert_eq(ta.live_bytes, 0u);
    cr_assert_eq(ta.misuse_count, 0u);
}

Test(support_allocator, blocks_are_aligned_and_writable)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);
    const size_t aligns[] = {1, 8, alignof(max_align_t), 64, 4'096};

    for (size_t i = 0; i < sizeof aligns / sizeof aligns[0]; ++i) {
        unsigned char *block = request(&ta, 33, aligns[i]);

        cr_assert_not_null(block);
        cr_assert_eq((uintptr_t)block % aligns[i], 0u, "align %zu", aligns[i]);
        memset(block, 0xFF, 33);
        release(&ta, block, 33);
    }
    cr_assert_eq(ta.live_bytes, 0u);
}

Test(support_allocator, free_with_wrong_size_is_a_misuse)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);
    void *block = request(&ta, 40, 8);

    release(&ta, block, 41);

    cr_assert_eq(ta.misuse_count, 1u);
    cr_assert_eq(ta.live_bytes, 0u, "the block is still released with its real size");
}

Test(support_allocator, zero_size_and_bad_alignment_are_misuses)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);

    cr_assert_null(request(&ta, 0, 8));
    cr_assert_null(request(&ta, 8, 3));

    cr_assert_eq(ta.misuse_count, 2u);
    cr_assert_eq(ta.alloc_count, 0u);
}

Test(support_allocator, free_of_nullptr_is_ignored)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);

    release(&ta, nullptr, 8);

    cr_assert_eq(ta.free_count, 0u);
    cr_assert_eq(ta.misuse_count, 0u);
}

Test(support_allocator, fail_at_zero_fails_only_the_first_request)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);
    ng_test_allocator_fail_at(&ta, 0);

    void *first = request(&ta, 8, 8);
    void *second = request(&ta, 8, 8);

    cr_assert_null(first);
    cr_assert_not_null(second);
    cr_assert_eq(ta.request_count, 2u);
    release(&ta, second, 8);
}

Test(support_allocator, fail_at_n_fails_the_request_of_that_index)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);
    void *blocks[3] = {};
    ng_test_allocator_fail_at(&ta, 2);

    for (size_t i = 0; i < 3; ++i) {
        blocks[i] = request(&ta, 8, 8);
    }

    cr_assert_not_null(blocks[0]);
    cr_assert_not_null(blocks[1]);
    cr_assert_null(blocks[2]);
    release(&ta, blocks[0], 8);
    release(&ta, blocks[1], 8);
}

Test(support_allocator, fail_at_resets_the_request_counter)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);
    void *before = request(&ta, 8, 8);

    ng_test_allocator_fail_at(&ta, 0);
    void *after = request(&ta, 8, 8);

    cr_assert_not_null(before);
    cr_assert_null(after);
    release(&ta, before, 8);
}

Test(support_allocator, cap_refuses_requests_above_the_limit)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);
    ng_test_allocator_cap(&ta, 100);

    void *fits = request(&ta, 60, 8);
    void *exceeds = request(&ta, 41, 8);
    void *fills = request(&ta, 40, 8);

    cr_assert_not_null(fits);
    cr_assert_null(exceeds);
    cr_assert_not_null(fills);
    cr_assert_eq(ta.live_bytes, 100u);
    release(&ta, fits, 60);
    release(&ta, fills, 40);
}

Test(support_allocator, cap_refuses_huge_requests_without_allocating)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);
    ng_test_allocator_cap(&ta, (size_t)1 << 20);

    void *block = request(&ta, SIZE_MAX, 1);

    cr_assert_null(block);
    cr_assert_eq(ta.live_bytes, 0u);
    cr_assert_eq(ta.misuse_count, 0u);
}

Test(support_allocator, oversized_request_without_cap_returns_nullptr)
{
    ng_test_allocator ta;
    ng_test_allocator_init(&ta);

    void *block = request(&ta, SIZE_MAX - 4u, 64);

    cr_assert_null(block);
    cr_assert_eq(ta.live_bytes, 0u);
}
