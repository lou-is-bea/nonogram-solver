#include <stdckdint.h>
#include <stddef.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

/* Capacity of the first content allocation; it then doubles. Covers a typical .sol line or
 * rendered row without reallocation. */
static constexpr size_t initial_capacity = 256;

struct ng_buffer {
    const ng_allocator *allocator;
    char *data;      /* nullptr until the first non-empty write */
    size_t size;     /* content bytes, terminating nul excluded */
    size_t capacity; /* bytes allocated for data, terminating nul included */
};

[[nodiscard]] ng_status ng_buffer_create(const ng_allocator *allocator, ng_buffer **out)
{
    if (out == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    if (allocator == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ng_buffer *buffer = allocator->alloc(allocator->ctx, sizeof *buffer, alignof(ng_buffer));
    if (buffer == nullptr) {
        return NG_ERR_OUT_OF_MEMORY;
    }
    *buffer = (ng_buffer){.allocator = allocator};
    *out = buffer;
    return NG_OK;
}

void ng_buffer_destroy(ng_buffer *buffer)
{
    if (buffer == nullptr) {
        return;
    }
    const ng_allocator *allocator = buffer->allocator;
    allocator->free(allocator->ctx, buffer->data, buffer->capacity);
    allocator->free(allocator->ctx, buffer, sizeof *buffer);
}

const char *ng_buffer_data(const ng_buffer *buffer)
{
    if (buffer == nullptr || buffer->data == nullptr) {
        return "";
    }
    return buffer->data;
}

size_t ng_buffer_size(const ng_buffer *buffer)
{
    return buffer == nullptr ? 0u : buffer->size;
}

/* Smallest capacity >= required obtained by doubling, falling back to required itself when
 * doubling would overflow. */
static size_t grown_capacity(size_t current, size_t required)
{
    size_t capacity = current == 0u ? initial_capacity : current;
    while (capacity < required) {
        if (ckd_mul(&capacity, capacity, (size_t)2)) {
            return required;
        }
    }
    return capacity;
}

/* Makes room for @p required bytes (terminating nul included), keeping the content. */
static ng_status reserve(ng_buffer *buffer, size_t required)
{
    if (required <= buffer->capacity) {
        return NG_OK;
    }
    const size_t capacity = grown_capacity(buffer->capacity, required);
    const ng_allocator *allocator = buffer->allocator;
    char *data = allocator->alloc(allocator->ctx, capacity, 1);
    if (data == nullptr) {
        return NG_ERR_OUT_OF_MEMORY;
    }
    if (buffer->data != nullptr) {
        memcpy(data, buffer->data, buffer->size + 1u);
        allocator->free(allocator->ctx, buffer->data, buffer->capacity);
    }
    buffer->data = data;
    buffer->capacity = capacity;
    return NG_OK;
}

static ng_status buffer_write(void *ctx, const char *buf, size_t len)
{
    if (ctx == nullptr || (buf == nullptr && len != 0u)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (len == 0u) {
        return NG_OK;
    }
    ng_buffer *buffer = ctx;
    size_t new_size = 0;
    size_t required = 0;
    if (ckd_add(&new_size, buffer->size, len) || ckd_add(&required, new_size, (size_t)1)) {
        return NG_ERR_LIMIT;
    }
    const ng_status status = reserve(buffer, required);
    if (status != NG_OK) {
        return status;
    }
    memcpy(buffer->data + buffer->size, buf, len);
    buffer->data[new_size] = '\0';
    buffer->size = new_size;
    return NG_OK;
}

ng_writer ng_writer_from_buffer(ng_buffer *buffer)
{
    return (ng_writer){.write = buffer_write, .ctx = buffer};
}
