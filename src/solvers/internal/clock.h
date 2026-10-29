/**
 * @file clock.h
 * @brief Conversion behind ng_clock_default(), exposed for tests. Internal.
 */
#ifndef NG_SOLVERS_INTERNAL_CLOCK_H
#define NG_SOLVERS_INTERNAL_CLOCK_H

#include <stdint.h>
#include <time.h>

/**
 * @brief Nanoseconds of a timespec_get() reading.
 * @param valid Whether timespec_get() succeeded.
 * @param now   The reading. Must not be nullptr.
 * @return 0 for a failed reading or a time before the epoch; UINT64_MAX when the value does
 *         not fit in 64 bits (about 584 years after the epoch); the exact value otherwise.
 */
uint64_t ngi_clock_ns_from_timespec(bool valid, const struct timespec *now);

#endif /* NG_SOLVERS_INTERNAL_CLOCK_H */
