/* clock_gettime() and CLOCK_MONOTONIC are POSIX: CMake defines _POSIX_C_SOURCE for this file
 * only, so the rest of the project stays strict C23 (decision of 2026-10-31, see
 * bench/README.md). C23 has TIME_MONOTONIC, but it is optional and glibc 2.39 lacks it. */
#include <stdckdint.h>
#include <stdint.h>
#include <time.h>

#include "nonogram/solver.h"

#include "bench.h"

static constexpr uint64_t ns_per_s = 1'000'000'000;

uint64_t ngi_bench_ns_from_timespec(bool valid, const struct timespec *now)
{
    if (!valid || now->tv_sec < 0 || now->tv_nsec < 0) {
        return 0;
    }
    /* Saturate rather than wrap; a monotonic reading counts from boot, far below 2^64 ns. */
    uint64_t ns = 0;
    if (ckd_mul(&ns, (uint64_t)now->tv_sec, ns_per_s) || ckd_add(&ns, ns, (uint64_t)now->tv_nsec)) {
        return UINT64_MAX;
    }
    return ns;
}

static uint64_t monotonic_now_ns(void *ctx)
{
    (void)ctx;
    struct timespec now = {};
    const bool valid = clock_gettime(CLOCK_MONOTONIC, &now) == 0;
    return ngi_bench_ns_from_timespec(valid, &now);
}

static const ng_clock monotonic_clock = {.now_ns = monotonic_now_ns, .ctx = nullptr};

const ng_clock *ngi_bench_clock(void)
{
    return &monotonic_clock;
}
