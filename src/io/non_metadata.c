#include "io/internal/non_metadata.h"

#include <stddef.h>

#include "nonogram/allocator.h"
#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

/* One growable buffer per field present; absent fields have none. */
struct ng_non_metadata {
    const ng_allocator *allocator;
    ng_buffer *fields[NG_NON_FIELD_COUNT];
};

static bool is_field(ng_non_field field)
{
    return (size_t)field < NG_NON_FIELD_COUNT;
}

[[nodiscard]] ng_status ngi_non_metadata_create(const ng_allocator *allocator,
                                                ng_non_metadata **out)
{
    *out = nullptr;
    ng_non_metadata *metadata =
        allocator->alloc(allocator->ctx, sizeof *metadata, alignof(ng_non_metadata));
    if (metadata == nullptr) {
        return NG_ERR_OUT_OF_MEMORY;
    }
    *metadata = (ng_non_metadata){.allocator = allocator};
    *out = metadata;
    return NG_OK;
}

[[nodiscard]] ng_status ngi_non_metadata_append(ng_non_metadata *metadata, ng_non_field field,
                                                const char *bytes, size_t size)
{
    ng_buffer **slot = &metadata->fields[field];
    if (*slot == nullptr) {
        const ng_status status = ng_buffer_create(metadata->allocator, slot);
        if (status != NG_OK) {
            return status;
        }
    }
    const ng_writer writer = ng_writer_from_buffer(*slot);
    return writer.write(writer.ctx, bytes, size);
}

const char *ng_non_metadata_get(const ng_non_metadata *metadata, ng_non_field field)
{
    if (metadata == nullptr || !is_field(field) || metadata->fields[field] == nullptr) {
        return nullptr;
    }
    return ng_buffer_data(metadata->fields[field]);
}

void ng_non_metadata_destroy(ng_non_metadata *metadata)
{
    if (metadata == nullptr) {
        return;
    }
    for (size_t i = 0; i < NG_NON_FIELD_COUNT; ++i) {
        ng_buffer_destroy(metadata->fields[i]);
    }
    metadata->allocator->free(metadata->allocator->ctx, metadata, sizeof *metadata);
}
