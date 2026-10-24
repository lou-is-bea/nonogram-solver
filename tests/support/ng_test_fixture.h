/**
 * @file ng_test_fixture.h
 * @brief Common Criterion fixture shared by every test suite (TESTING.md §4.1).
 *
 * Usage: Test(suite, name, .init = ng_test_setup, .fini = ng_test_teardown).
 *
 * In M0 the fixture only establishes the hook points. M1 adds the counting allocator to it,
 * at which point ng_test_teardown fails any test that leaves memory allocated.
 */
#ifndef NG_TESTS_SUPPORT_NG_TEST_FIXTURE_H
#define NG_TESTS_SUPPORT_NG_TEST_FIXTURE_H

/** @brief Per-test initialisation, run by Criterion before the test body. */
void ng_test_setup(void);

/** @brief Per-test checks and cleanup, run by Criterion after the test body. */
void ng_test_teardown(void);

#endif /* NG_TESTS_SUPPORT_NG_TEST_FIXTURE_H */
