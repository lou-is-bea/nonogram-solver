#include "test_rng.h"

#include <criterion/criterion.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

/* Reference algorithms: https://prng.di.unimi.it/ (Blackman and Vigna, public domain).
 *
 * They rely on arithmetic modulo 2^64. UBSan's -fsanitize=integer reports every unsigned
 * wraparound, and the project adds no sanitizer suppression, so the wrapping operations are
 * written so that no intermediate result exceeds 64 bits. */

static constexpr uint64_t low_32_bits = 0xFFFF'FFFFu;

static uint64_t wrapping_add(uint64_t a, uint64_t b)
{
    const uint64_t room = UINT64_MAX - a;
    return b > room ? b - room - 1u : a + b;
}

/* (a * b) mod 2^64 from 32-bit halves: the a_hi * b_hi term vanishes modulo 2^64. */
static uint64_t wrapping_mul(uint64_t a, uint64_t b)
{
    const uint64_t a_lo = a & low_32_bits;
    const uint64_t a_hi = a >> 32;
    const uint64_t b_lo = b & low_32_bits;
    const uint64_t b_hi = b >> 32;
    const uint64_t low = a_lo * b_lo;
    const uint64_t cross = wrapping_add((a_hi * b_lo) & low_32_bits, (a_lo * b_hi) & low_32_bits);
    return wrapping_add(low, (cross & low_32_bits) << 32);
}

/* (value << shift) mod 2^64, dropping the high bits before shifting. */
static uint64_t shift_left(uint64_t value, int shift)
{
    return (value & (UINT64_MAX >> shift)) << shift;
}

static uint64_t rotate_left(uint64_t value, int shift)
{
    return shift_left(value, shift) | (value >> (64 - shift));
}

static uint64_t splitmix64(uint64_t *state)
{
    *state = wrapping_add(*state, 0x9E3779B97F4A7C15u);
    uint64_t z = *state;
    z = wrapping_mul(z ^ (z >> 30), 0xBF58476D1CE4E5B9u);
    z = wrapping_mul(z ^ (z >> 27), 0x94D049BB133111EBu);
    return z ^ (z >> 31);
}

void ng_test_rng_seed(ng_test_rng *rng, uint64_t seed)
{
    uint64_t sm = seed;
    for (int i = 0; i < 4; ++i) {
        rng->state[i] = splitmix64(&sm);
    }
}

uint64_t ng_test_rng_next(ng_test_rng *rng)
{
    uint64_t *s = rng->state;
    const uint64_t result = wrapping_mul(rotate_left(wrapping_mul(s[1], 5u), 7), 9u);
    const uint64_t t = shift_left(s[1], 17);
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotate_left(s[3], 45);
    return result;
}

uint64_t ng_test_rng_below(ng_test_rng *rng, uint64_t bound)
{
    cr_assert_neq(bound, 0u, "ng_test_rng_below: bound must not be 0");
    /* Rejection sampling: drop the top values that would make some residues more likely. */
    const uint64_t limit = UINT64_MAX - (UINT64_MAX % bound);
    uint64_t value = 0;
    do {
        value = ng_test_rng_next(rng);
    } while (value >= limit);
    return value % bound;
}

uint64_t ng_test_seed(void)
{
    uint64_t seed = NG_TEST_DEFAULT_SEED;
    const char *text = getenv("NG_TEST_SEED");
    if (text != nullptr) {
        char *end = nullptr;
        errno = 0;
        const unsigned long long parsed = strtoull(text, &end, 0);
        cr_assert(errno == 0 && end != text && *end == '\0', "malformed NG_TEST_SEED: '%s'", text);
        seed = (uint64_t)parsed;
    }
    cr_log_info("seed: %" PRIu64 " (replay with NG_TEST_SEED=%" PRIu64 ")", seed, seed);
    return seed;
}
