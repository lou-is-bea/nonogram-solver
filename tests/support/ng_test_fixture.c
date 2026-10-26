#include "ng_test_fixture.h"

#include <criterion/criterion.h>

#include "test_allocator.h"

/* Criterion fixtures take no argument, so the allocator lives at file scope. Each test runs
 * in its own process, so it never leaks from one test to the next. */
static ng_test_allocator heap;

void ng_test_setup(void)
{
    ng_test_allocator_init(&heap);
}

/* Criterion 2.4 does not fail a test for an assertion raised in .fini: it only prints it. The
 * "NG_FIXTURE_FAILURE" marker is what makes CTest fail the test (tests/CMakeLists.txt). */
void ng_test_teardown(void)
{
    if (heap.live_bytes != 0u) {
        cr_log_error("NG_FIXTURE_FAILURE: memory leak, %zu bytes still allocated", heap.live_bytes);
    }
    if (heap.misuse_count != 0u) {
        cr_log_error("NG_FIXTURE_FAILURE: allocator contract violated %" NG_PRI_COUNTER " time(s)",
                     heap.misuse_count);
    }
}

ng_test_allocator *ng_test_heap(void)
{
    return &heap;
}
