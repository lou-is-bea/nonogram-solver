/**
 * @file deadline.h
 * @brief Time budget of a solve (max_time_ms), shared by the solvers. Internal.
 *
 * A solver starts the deadline at the beginning of solve() and checks it from time to time;
 * each check reads the clock once. Nothing is allocated and no state is global.
 */
#ifndef NG_SOLVERS_COMMON_DEADLINE_H
#define NG_SOLVERS_COMMON_DEADLINE_H

#include <stdint.h>

#include "nonogram/solver.h"

typedef struct ngi_deadline {
    const ng_clock *clock; /**< Clock of the options, or ng_clock_default(). */
    uint64_t end_ns;       /**< Reading at which the budget is spent. */
    bool active;           /**< false: no limit, or an end beyond the clock range. */
} ngi_deadline;

/**
 * @brief Start the budget of @p options now.
 *
 * Without a time limit, or when the end does not fit in 64 bits, the deadline never passes.
 * A missing clock, or one without now_ns, means ng_clock_default().
 *
 * @param deadline Receives the deadline. Must not be nullptr.
 * @param options  Options of the solve. Must not be nullptr; the clock is borrowed.
 */
void ngi_deadline_start(ngi_deadline *deadline, const ng_solve_options *options);

/**
 * @brief Whether the budget is spent.
 * @param deadline Started deadline. Must not be nullptr.
 * @return true once the clock reaches the end; always false for an inactive deadline.
 */
bool ngi_deadline_passed(const ngi_deadline *deadline);

#endif /* NG_SOLVERS_COMMON_DEADLINE_H */
