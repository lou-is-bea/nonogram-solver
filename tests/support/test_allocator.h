/**
 * @file test_allocator.h
 * @brief Instrumented allocator for tests: counting, failing at the n-th request, capped.
 *
 * One object covers the three test allocators of TESTING.md §4.3 and ADR-0004:
 * - counting: live and peak bytes, number of calls, and detection of a free whose size
 *   differs from the allocation (always on);
 * - failing: ng_test_allocator_fail_at() makes exactly the n-th request (0-based) fail;
 * - capped: ng_test_allocator_cap() refuses any request that would push the live bytes
 *   above a limit, so huge sizes are exercised without allocating them.
 *
 * Pass &allocator.base to the code under test. Blocks are served by ng_allocator_default()
 * with a hidden header recording the requested size, so a free with a wrong size is detected
 * (and the block still released with its real size).
 */
#ifndef NG_TESTS_SUPPORT_TEST_ALLOCATOR_H
#define NG_TESTS_SUPPORT_TEST_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/types.h"

/** @brief fail_at value meaning "never fail". */
constexpr ng_counter NG_TEST_NEVER_FAIL = UINT64_MAX;

/** @brief cap_bytes value meaning "no cap". */
constexpr size_t NG_TEST_NO_CAP = SIZE_MAX;

typedef struct ng_test_allocator {
    ng_allocator base;        /**< Interface given to the code under test. */
    size_t live_bytes;        /**< Bytes currently allocated (requested sizes). */
    size_t peak_bytes;        /**< Highest value reached by live_bytes. */
    ng_counter request_count; /**< alloc calls since init or the last fail_at(). */
    ng_counter alloc_count;   /**< Successful allocations since init. */
    ng_counter free_count;    /**< Non-null frees since init. */
    ng_counter misuse_count;  /**< Contract violations: zero size, bad align, wrong free size. */
    ng_counter fail_at;       /**< Index of the request that fails, or NG_TEST_NEVER_FAIL. */
    size_t cap_bytes;         /**< Limit on live_bytes, or NG_TEST_NO_CAP. */
} ng_test_allocator;

/** @brief Initialise @p ta as a counting allocator that never fails and has no cap. */
void ng_test_allocator_init(ng_test_allocator *ta);

/**
 * @brief Make request number @p n (0-based, counted from now) fail, and only that one.
 *
 * Resets request_count. NG_TEST_NEVER_FAIL disarms the failure.
 */
void ng_test_allocator_fail_at(ng_test_allocator *ta, ng_counter n);

/** @brief Refuse requests that would push live_bytes above @p cap_bytes. */
void ng_test_allocator_cap(ng_test_allocator *ta, size_t cap_bytes);

#endif /* NG_TESTS_SUPPORT_TEST_ALLOCATOR_H */
