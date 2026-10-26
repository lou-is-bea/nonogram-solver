#include <criterion/criterion.h>
#include <string.h>

#include "nonogram/status.h"

#include "ng_test_fixture.h"

TestSuite(status_str, .init = ng_test_setup, .fini = ng_test_teardown);

static const ng_status all_statuses[] = {
    NG_OK,     NG_ERR_INVALID_ARGUMENT, NG_ERR_OUT_OF_MEMORY, NG_ERR_LIMIT,    NG_ERR_PARSE,
    NG_ERR_IO, NG_ERR_INCONSISTENT,     NG_ERR_NOT_FOUND,     NG_ERR_INTERNAL,
};
static constexpr size_t status_count = sizeof all_statuses / sizeof all_statuses[0];

Test(status_str, ok_is_zero)
{
    cr_assert_eq((int)NG_OK, 0);
}

Test(status_str, ok_reads_ok)
{
    cr_assert_str_eq(ng_status_str(NG_OK), "ok");
}

Test(status_str, out_of_memory_reads_out_of_memory)
{
    cr_assert_str_eq(ng_status_str(NG_ERR_OUT_OF_MEMORY), "out of memory");
}

Test(status_str, every_status_has_a_known_description)
{
    for (size_t i = 0; i < status_count; ++i) {
        const char *text = ng_status_str(all_statuses[i]);

        cr_assert_not_null(text);
        cr_assert_gt(strlen(text), 0u);
        cr_assert_str_neq(text, "unknown status", "status %zu has no description", i);
    }
}

Test(status_str, descriptions_are_distinct)
{
    for (size_t i = 0; i < status_count; ++i) {
        for (size_t j = i + 1; j < status_count; ++j) {
            cr_assert_str_neq(ng_status_str(all_statuses[i]), ng_status_str(all_statuses[j]));
        }
    }
}

Test(status_str, value_outside_the_enumeration_is_unknown)
{
    /* Deliberately out of range: any uint8_t is a valid ng_status object (fixed type). */
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    const ng_status bogus = (ng_status)(NG_ERR_INTERNAL + 1);

    cr_assert_str_eq(ng_status_str(bogus), "unknown status");
}

Test(status_str, largest_underlying_value_is_unknown)
{
    /* Deliberately out of range: any uint8_t is a valid ng_status object (fixed type). */
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    const ng_status bogus = (ng_status)UINT8_MAX;

    cr_assert_str_eq(ng_status_str(bogus), "unknown status");
}
