#include <criterion/criterion.h>
#include <stdint.h>
#include <stdlib.h>

#include "ng_test_fixture.h"
#include "test_rng.h"

/* Every property test draws its inputs from this generator: it is tested like library code. */
TestSuite(support_rng, .init = ng_test_setup, .fini = ng_test_teardown);

Test(support_rng, matches_the_reference_sequence_for_seed_zero)
{
    /* Expected values computed with an independent implementation of splitmix64 and
     * xoshiro256** (Blackman and Vigna). */
    const uint64_t expected[] = {0x99EC5F36CB75F2B4u, 0xBF6E1F784956452Au, 0x1A5F849D4933E6E0u,
                                 0x6AA594F1262D2D2Cu};
    ng_test_rng rng;
    ng_test_rng_seed(&rng, 0);

    for (size_t i = 0; i < sizeof expected / sizeof expected[0]; ++i) {
        cr_assert_eq(ng_test_rng_next(&rng), expected[i], "output %zu", i);
    }
}

Test(support_rng, same_seed_gives_the_same_sequence)
{
    ng_test_rng a;
    ng_test_rng b;
    ng_test_rng_seed(&a, 42);
    ng_test_rng_seed(&b, 42);

    for (int i = 0; i < 100; ++i) {
        cr_assert_eq(ng_test_rng_next(&a), ng_test_rng_next(&b));
    }
}

Test(support_rng, below_stays_in_range_and_reaches_every_value)
{
    ng_test_rng rng;
    ng_test_rng_seed(&rng, 7);
    bool seen[7] = {};

    for (int i = 0; i < 1'000; ++i) {
        const uint64_t value = ng_test_rng_below(&rng, 7);
        cr_assert_lt(value, 7u);
        seen[value] = true;
    }

    for (size_t v = 0; v < 7; ++v) {
        cr_assert(seen[v], "value %zu never drawn", v);
    }
}

Test(support_rng, below_one_is_always_zero)
{
    ng_test_rng rng;
    ng_test_rng_seed(&rng, 1);

    cr_assert_eq(ng_test_rng_below(&rng, 1), 0u);
}

Test(support_rng, default_seed_is_used_without_environment)
{
    /* CTest does not set NG_TEST_SEED; a developer replaying a failure may. */
    if (getenv("NG_TEST_SEED") != nullptr) {
        cr_skip_test("NG_TEST_SEED is set");
    }

    cr_assert_eq(ng_test_seed(), NG_TEST_DEFAULT_SEED);
}
