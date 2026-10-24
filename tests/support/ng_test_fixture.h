/**
 * @file ng_test_fixture.h
 * @brief Common Criterion fixture shared by every test suite (TESTING.md §4.1).
 *
 * Usage: TestSuite(suite, .init = ng_test_setup, .fini = ng_test_teardown).
 *
 * The fixture installs a fresh counting allocator before each test. Tests pass
 * &ng_test_heap()->base to the code under test; ng_test_teardown fails the test if any of
 * that memory is still allocated, or if the allocator contract was violated.
 */
#ifndef NG_TESTS_SUPPORT_NG_TEST_FIXTURE_H
#define NG_TESTS_SUPPORT_NG_TEST_FIXTURE_H

#include "test_allocator.h"

/** @brief Per-test initialisation, run by Criterion before the test body. */
void ng_test_setup(void);

/** @brief Per-test leak and misuse checks, run by Criterion after the test body. */
void ng_test_teardown(void);

/** @brief The current test's counting allocator. Valid between setup and teardown. */
ng_test_allocator *ng_test_heap(void);

#endif /* NG_TESTS_SUPPORT_NG_TEST_FIXTURE_H */
