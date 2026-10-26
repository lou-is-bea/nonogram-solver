#include "io/internal/char_source.h"

#include <stddef.h>

#include "nonogram/allocator.h"
#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

#include "core/internal/assert.h"

[[nodiscard]] ng_status ngi_char_source_init(ngi_char_source *source, const ng_allocator *allocator,
                                             const ng_reader *reader)
{
    char *block = allocator->alloc(allocator->ctx, NG_IO_BLOCK_SIZE, 1);
    if (block == nullptr) {
        return NG_ERR_OUT_OF_MEMORY;
    }
    *source = (ngi_char_source){.reader = reader,
                                .block = block,
                                .capacity = NG_IO_BLOCK_SIZE,
                                .line = 1,
                                .column = 1,
                                .status = NG_OK};
    return NG_OK;
}

void ngi_char_source_release(ngi_char_source *source, const ng_allocator *allocator)
{
    allocator->free(allocator->ctx, source->block, source->capacity);
    source->block = nullptr;
}

/* Refills the block once it is exhausted. A read of zero bytes means the end of the input
 * (ng_reader contract), so a single read is enough: data, end or error. */
static void refill(ngi_char_source *source)
{
    if (source->next != source->size || source->at_end || source->status != NG_OK) {
        return;
    }
    size_t got = 0;
    const ng_status status =
        source->reader->read(source->reader->ctx, source->block, source->capacity, &got);
    if (status != NG_OK) {
        source->status = status;
    } else if (got == 0u) {
        source->at_end = true;
    } else {
        source->size = got;
        source->next = 0;
    }
}

int ngi_char_source_peek(ngi_char_source *source)
{
    refill(source);
    if (source->status != NG_OK) {
        return NGI_CHAR_FAILED;
    }
    if (source->next == source->size) {
        return NGI_CHAR_END;
    }
    return (unsigned char)source->block[source->next];
}

void ngi_char_source_advance(ngi_char_source *source)
{
    NG_ASSERT(source->next < source->size);
    if (source->block[source->next] == '\n') {
        ++source->line;
        source->column = 1;
    } else {
        ++source->column;
    }
    ++source->next;
}
