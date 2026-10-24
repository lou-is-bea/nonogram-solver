/**
 * @file assert.h
 * @brief Internal invariant checks (ngi). Never used to validate caller input.
 *
 * NG_ASSERT is active in Debug builds and compiles to nothing when NDEBUG is defined
 * (Release). NG_UNREACHABLE marks a point the code can never reach: it asserts in Debug and
 * lets the optimiser assume it in Release.
 */
#ifndef NG_CORE_INTERNAL_ASSERT_H
#define NG_CORE_INTERNAL_ASSERT_H

#include <assert.h>
#include <stddef.h>

#define NG_ASSERT(cond) assert(cond)

#define NG_UNREACHABLE()                                                                           \
    do {                                                                                           \
        assert(false && "unreachable");                                                            \
        unreachable();                                                                             \
    } while (false)

#endif /* NG_CORE_INTERNAL_ASSERT_H */
