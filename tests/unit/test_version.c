#include <criterion/criterion.h>
#include <inttypes.h>
#include <stdio.h>

#include "nonogram/version.h"

#include "ng_test_fixture.h"

TestSuite(version_string, .init = ng_test_setup, .fini = ng_test_teardown);

Test(version_string, never_returns_nullptr)
{
    const char *version = ng_version_string();

    cr_assert_not_null(version);
}

Test(version_string, matches_header_components)
{
    constexpr size_t version_capacity = 32;
    char expected[version_capacity] = {};
    const int written = snprintf(expected, sizeof expected, "%" PRIu32 ".%" PRIu32 ".%" PRIu32,
                                 NG_VERSION_MAJOR, NG_VERSION_MINOR, NG_VERSION_PATCH);
    cr_assert_gt(written, 0);
    cr_assert_lt((size_t)written, sizeof expected);

    const char *version = ng_version_string();

    cr_assert_str_eq(version, expected);
}

Test(version_string, returns_the_same_static_string)
{
    const char *first = ng_version_string();

    const char *second = ng_version_string();

    cr_assert_eq(first, second);
}

#include <stdlib.h>

Test(version_string, leaks_a_block)
{
    /* Unreachable once the test returns: LeakSanitizer must report it. */
    char *volatile block = malloc(64);

    cr_assert_not_null(block);
}
