#include "solvers/internal/deadline.h"

#include <stdckdint.h>
#include <stdint.h>

static constexpr uint64_t ns_per_ms = 1000000u;

void ngi_deadline_start(ngi_deadline *deadline, const ng_solve_options *options)
{
    deadline->clock = options->clock != nullptr ? options->clock : ng_clock_default();
    deadline->end_ns = 0;
    deadline->active = false;
    if (options->max_time_ms == 0u) {
        return;
    }
    const uint64_t start = deadline->clock->now_ns(deadline->clock->ctx);
    uint64_t budget = 0;
    uint64_t end = 0;
    if (ckd_mul(&budget, options->max_time_ms, ns_per_ms) || ckd_add(&end, start, budget)) {
        return; /* the clock never reaches this end: same as no limit */
    }
    deadline->end_ns = end;
    deadline->active = true;
}

bool ngi_deadline_passed(const ngi_deadline *deadline)
{
    return deadline->active && deadline->clock->now_ns(deadline->clock->ctx) >= deadline->end_ns;
}
