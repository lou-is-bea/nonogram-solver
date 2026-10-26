/**
 * @file non_metadata.h
 * @brief Construction of ng_non_metadata by the .non reader (ngi).
 */
#ifndef NG_IO_INTERNAL_NON_METADATA_H
#define NG_IO_INTERNAL_NON_METADATA_H

#include <stddef.h>

#include "nonogram/allocator.h"
#include "nonogram/io.h"
#include "nonogram/status.h"

/** @brief Create metadata with every field absent. */
[[nodiscard]] ng_status ngi_non_metadata_create(const ng_allocator *allocator,
                                                ng_non_metadata **out);

/**
 * @brief Append bytes to a field, making it present even when @p size is 0.
 * @return NG_OK, NG_ERR_OUT_OF_MEMORY or NG_ERR_LIMIT; on failure the field is unchanged.
 */
[[nodiscard]] ng_status ngi_non_metadata_append(ng_non_metadata *metadata, ng_non_field field,
                                                const char *bytes, size_t size);

#endif /* NG_IO_INTERNAL_NON_METADATA_H */
