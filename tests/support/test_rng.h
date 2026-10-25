/**
 * @file test_rng.h
 * @brief Seeded pseudo-random generator for tests (xoshiro256**, seeded by splitmix64).
 *
 * Every random test takes its seed from ng_test_seed(), which logs it, so that any failure
 * can be replayed with NG_TEST_SEED=<seed> (TESTING.md §4.4). rand() is forbidden.
 */
#ifndef NG_TESTS_SUPPORT_TEST_RNG_H
#define NG_TESTS_SUPPORT_TEST_RNG_H

#include <stdint.h>

typedef struct ng_test_rng {
    uint64_t state[4];
} ng_test_rng;

/** @brief Seed used when NG_TEST_SEED is not set. */
constexpr uint64_t NG_TEST_DEFAULT_SEED = 0x6E6F6E6F6772616Du; /* "nonogram" */

/**
 * @brief Return the seed of the current test run and log it.
 *
 * Reads NG_TEST_SEED (decimal or 0x-prefixed hexadecimal) when set; a malformed value fails
 * the test. Falls back to NG_TEST_DEFAULT_SEED.
 */
uint64_t ng_test_seed(void);

/**
 * @brief Number of cases a property test runs: NG_PROPERTY_CASES when set (for example
 *        100000 in the nightly run), @p default_count otherwise. Logged like the seed.
 */
uint64_t ng_test_case_count(uint64_t default_count);

/** @brief Initialise @p rng from @p seed. Any seed, including 0, is valid. */
void ng_test_rng_seed(ng_test_rng *rng, uint64_t seed);

/** @brief Next 64 random bits. */
uint64_t ng_test_rng_next(ng_test_rng *rng);

/** @brief Uniform value in [0, bound), without modulo bias. @p bound must not be 0. */
uint64_t ng_test_rng_below(ng_test_rng *rng, uint64_t bound);

#endif /* NG_TESTS_SUPPORT_TEST_RNG_H */
