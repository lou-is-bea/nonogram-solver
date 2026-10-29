#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "nonogram/solver.h"

#include "ng_test_fixture.h"
#include "solvers/common/deadline.h"
#include "solvers/internal/clock.h"
#include "solvers/internal/registry.h"

TestSuite(solver_deadline, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(solver_registry_list, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(solver_clock, .init = ng_test_setup, .fini = ng_test_teardown);

/* A clock whose reading the test sets directly. */
static uint64_t manual_now(void *ctx)
{
    return *(const uint64_t *)ctx;
}

static constexpr uint64_t ns_per_ms = 1000000u;

Test(solver_deadline, without_time_limit_never_passes)
{
    uint64_t now = 0;
    const ng_clock clock = {.now_ns = manual_now, .ctx = &now};
    ng_solve_options options = ng_solve_options_default();
    options.clock = &clock;
    ngi_deadline deadline;

    ngi_deadline_start(&deadline, &options);
    now = UINT64_MAX;

    cr_assert_not(ngi_deadline_passed(&deadline));
}

Test(solver_deadline, passes_exactly_when_the_budget_is_spent)
{
    uint64_t now = 1000;
    const ng_clock clock = {.now_ns = manual_now, .ctx = &now};
    ng_solve_options options = ng_solve_options_default();
    options.clock = &clock;
    options.max_time_ms = 5;
    ngi_deadline deadline;

    ngi_deadline_start(&deadline, &options);

    now = 1000 + 5 * ns_per_ms - 1;
    cr_assert_not(ngi_deadline_passed(&deadline));
    now = 1000 + 5 * ns_per_ms;
    cr_assert(ngi_deadline_passed(&deadline));
}

Test(solver_deadline, unrepresentable_end_never_passes)
{
    uint64_t now = UINT64_MAX - 10;
    const ng_clock clock = {.now_ns = manual_now, .ctx = &now};
    ng_solve_options options = ng_solve_options_default();
    options.clock = &clock;
    options.max_time_ms = UINT64_MAX; /* overflows once converted to nanoseconds */
    ngi_deadline deadline;

    ngi_deadline_start(&deadline, &options);
    now = UINT64_MAX;

    cr_assert_not(ngi_deadline_passed(&deadline));
}

Test(solver_deadline, end_beyond_the_clock_range_never_passes)
{
    uint64_t now = UINT64_MAX - 10;
    const ng_clock clock = {.now_ns = manual_now, .ctx = &now};
    ng_solve_options options = ng_solve_options_default();
    options.clock = &clock;
    options.max_time_ms = 1; /* start + 1 ms does not fit in 64 bits */
    ngi_deadline deadline;

    ngi_deadline_start(&deadline, &options);
    now = UINT64_MAX;

    cr_assert_not(ngi_deadline_passed(&deadline));
}

Test(solver_deadline, null_clock_uses_the_default_clock)
{
    ng_solve_options options = ng_solve_options_default();
    options.max_time_ms = UINT64_C(3600000); /* one hour: cannot pass during the test */
    ngi_deadline deadline;

    ngi_deadline_start(&deadline, &options);

    cr_assert_eq(deadline.clock, ng_clock_default());
    cr_assert_not(ngi_deadline_passed(&deadline));
}

/* --- solver_registry_list ------------------------------------------------------------------- */

static const ng_solver_vtable first = {.name = "first", .summary = "1"};
static const ng_solver_vtable second = {.name = "second", .summary = "2"};
static const ng_solver_vtable second_again = {.name = "second", .summary = "shadowed"};
static const ng_solver_vtable *const two_entries[] = {&first, &second, &second_again, nullptr};
static const ng_solver_vtable *const no_entry[] = {nullptr};

Test(solver_registry_list, count_stops_at_the_terminator)
{
    cr_assert_eq(ngi_registry_count(two_entries), 3u);
    cr_assert_eq(ngi_registry_count(no_entry), 0u);
}

Test(solver_registry_list, at_returns_each_entry_then_nullptr)
{
    cr_assert_eq(ngi_registry_at(two_entries, 0), &first);
    cr_assert_eq(ngi_registry_at(two_entries, 1), &second);
    cr_assert_eq(ngi_registry_at(two_entries, 2), &second_again);
    cr_assert_null(ngi_registry_at(two_entries, 3));
    cr_assert_null(ngi_registry_at(two_entries, SIZE_MAX));
    cr_assert_null(ngi_registry_at(no_entry, 0));
}

Test(solver_registry_list, find_returns_the_first_entry_with_that_name)
{
    cr_assert_eq(ngi_registry_find(two_entries, "first"), &first);
    cr_assert_eq(ngi_registry_find(two_entries, "second"), &second);
    cr_assert_null(ngi_registry_find(two_entries, "secon"));
    cr_assert_null(ngi_registry_find(two_entries, "seconds"));
    cr_assert_null(ngi_registry_find(two_entries, nullptr));
    cr_assert_null(ngi_registry_find(no_entry, "first"));
}

Test(solver_deadline, clock_without_reading_function_falls_back_to_the_default)
{
    const ng_clock broken = {.now_ns = nullptr, .ctx = nullptr};
    ng_solve_options options = ng_solve_options_default();
    options.clock = &broken;
    options.max_time_ms = UINT64_C(3600000);
    ngi_deadline deadline;

    ngi_deadline_start(&deadline, &options);

    cr_assert_eq(deadline.clock, ng_clock_default());
    cr_assert_not(ngi_deadline_passed(&deadline));
}

/* --- solver_clock --------------------------------------------------------------------------- */

Test(solver_clock, failed_reading_gives_zero)
{
    const struct timespec now = {.tv_sec = 10, .tv_nsec = 5};
    cr_assert_eq(ngi_clock_ns_from_timespec(false, &now), 0u);
}

Test(solver_clock, time_before_the_epoch_gives_zero)
{
    const struct timespec before = {.tv_sec = -1, .tv_nsec = 0};
    const struct timespec bad_nanoseconds = {.tv_sec = 1, .tv_nsec = -1};
    cr_assert_eq(ngi_clock_ns_from_timespec(true, &before), 0u);
    cr_assert_eq(ngi_clock_ns_from_timespec(true, &bad_nanoseconds), 0u);
}

Test(solver_clock, reading_is_converted_exactly)
{
    const struct timespec now = {.tv_sec = 1700000000, .tv_nsec = 123456789};
    cr_assert_eq(ngi_clock_ns_from_timespec(true, &now), UINT64_C(1700000000123456789));
}

Test(solver_clock, largest_representable_reading_is_exact_and_beyond_saturates)
{
    /* UINT64_MAX = 18446744073 s + 709551615 ns. */
    const struct timespec largest = {.tv_sec = 18446744073, .tv_nsec = 709551615};
    const struct timespec one_more = {.tv_sec = 18446744073, .tv_nsec = 709551616};
    const struct timespec next_second = {.tv_sec = 18446744074, .tv_nsec = 0};
    cr_assert_eq(ngi_clock_ns_from_timespec(true, &largest), UINT64_MAX);
    cr_assert_eq(ngi_clock_ns_from_timespec(true, &one_more), UINT64_MAX);
    cr_assert_eq(ngi_clock_ns_from_timespec(true, &next_second), UINT64_MAX);
}
