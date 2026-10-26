/**
 * @file status.h
 * @brief Error reporting: every library function that can fail returns an ng_status.
 */
#ifndef NG_NONOGRAM_STATUS_H
#define NG_NONOGRAM_STATUS_H

#include <stdint.h>

/** @brief Outcome of a library call. NG_OK is zero; every other value is an error. */
enum ng_status : uint8_t {
    NG_OK = 0,               /**< Success. */
    NG_ERR_INVALID_ARGUMENT, /**< A caller-supplied argument violates the documented contract. */
    NG_ERR_OUT_OF_MEMORY,    /**< The allocator refused a request. */
    NG_ERR_LIMIT,            /**< A dimension or size is not representable, or a limit is hit. */
    NG_ERR_PARSE,            /**< Malformed input data. */
    NG_ERR_IO,               /**< A read or write on the underlying stream failed. */
    NG_ERR_INCONSISTENT,     /**< The puzzle is trivially impossible (clues do not fit). */
    NG_ERR_NOT_FOUND,        /**< The requested item (solver, line, ...) does not exist. */
    NG_ERR_INTERNAL,         /**< A broken internal invariant: a library bug. */
};
typedef enum ng_status ng_status;

/**
 * @brief Return a short English description of a status, for diagnostics.
 *
 * @param status Any value, including values outside the enumeration.
 * @return A static, nul-terminated string such as "out of memory"; "unknown status" for a
 *         value outside the enumeration. Never nullptr.
 * @ownership The string is owned by the library and must not be freed or modified.
 * @complexity O(1) time and memory.
 */
const char *ng_status_str(ng_status status);

#endif /* NG_NONOGRAM_STATUS_H */
