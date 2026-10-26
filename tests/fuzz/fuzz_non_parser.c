/* libFuzzer target for ng_io_read_non (TESTING.md §4.7).
 *
 * Beyond "no crash, no sanitizer report", every input checks:
 * - on failure: no object is returned and the message is a nul-terminated string;
 * - on success: writing the puzzle and reading it back gives the same clues and metadata
 *   (P-IO2 on arbitrary inputs);
 * - in both cases: nothing allocated by the library is left behind.
 * A violated check calls abort(), which libFuzzer reports as a crash with its input. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

/* Enough for any puzzle a fuzzer input can describe in practice; a larger request is refused
 * (as an allocation failure) instead of exhausting the machine. */
static constexpr size_t memory_cap = (size_t)64 << 20;

typedef struct capped_heap {
    size_t live;
} capped_heap;

static void *capped_alloc(void *ctx, size_t size, size_t align)
{
    capped_heap *heap = ctx;
    if (size > memory_cap - heap->live) {
        return nullptr;
    }
    const ng_allocator *system = ng_allocator_default();
    void *block = system->alloc(system->ctx, size, align);
    if (block != nullptr) {
        heap->live += size;
    }
    return block;
}

static void capped_free(void *ctx, void *ptr, size_t size)
{
    capped_heap *heap = ctx;
    if (ptr != nullptr) {
        heap->live -= size;
        ng_allocator_default()->free(nullptr, ptr, size);
    }
}

static void check(bool condition)
{
    if (!condition) {
        abort();
    }
}

static bool same_text(const char *a, const char *b)
{
    return (a == nullptr) == (b == nullptr) && (a == nullptr || strcmp(a, b) == 0);
}

/* True when line @p line has the same runs in both puzzles. The clues are copied into buffers
 * taken from the capped heap; the target never allocates outside it. */
static bool same_line(const ng_allocator *heap, const ng_puzzle *a, const ng_puzzle *b,
                      ng_line_id line)
{
    const ng_run_count count = ng_puzzle_line_run_count(a, line);
    if (ng_puzzle_line_run_count(b, line) != count) {
        return false;
    }
    if (count == 0u) {
        return true;
    }
    const size_t bytes = (size_t)count * sizeof(ng_dim);
    ng_dim *runs_a = heap->alloc(heap->ctx, bytes, alignof(ng_dim));
    ng_dim *runs_b = heap->alloc(heap->ctx, bytes, alignof(ng_dim));
    /* Both puzzles already fit in the budget, so one line of each does too. */
    check(runs_a != nullptr && runs_b != nullptr);
    check(ng_puzzle_copy_line_runs(a, line, runs_a, count) == NG_OK);
    check(ng_puzzle_copy_line_runs(b, line, runs_b, count) == NG_OK);
    const bool same = memcmp(runs_a, runs_b, bytes) == 0;
    heap->free(heap->ctx, runs_a, bytes);
    heap->free(heap->ctx, runs_b, bytes);
    return same;
}

static bool same_puzzle(const ng_allocator *heap, const ng_puzzle *a, const ng_puzzle *b)
{
    if (ng_puzzle_width(a) != ng_puzzle_width(b) || ng_puzzle_height(a) != ng_puzzle_height(b) ||
        ng_puzzle_total_runs(a) != ng_puzzle_total_runs(b)) {
        return false;
    }
    for (ng_line_id line = 0; line < ng_puzzle_line_count(a); ++line) {
        if (!same_line(heap, a, b, line)) {
            return false;
        }
    }
    return true;
}

static void check_round_trip(const ng_allocator *a, const ng_puzzle *puzzle,
                             const ng_non_metadata *metadata)
{
    ng_buffer *text = nullptr;
    if (ng_buffer_create(a, &text) != NG_OK) {
        return; /* out of the fuzzing memory budget: nothing to compare */
    }
    const ng_writer w = ng_writer_from_buffer(text);
    if (ng_io_write_non(puzzle, metadata, &w) == NG_OK) {
        ng_memory_source source;
        const ng_reader r =
            ng_reader_from_memory(&source, ng_buffer_data(text), ng_buffer_size(text));
        ng_puzzle *back = nullptr;
        ng_non_metadata *back_metadata = nullptr;
        const ng_status status = ng_io_read_non(a, &r, &back, &back_metadata, nullptr);
        check(status == NG_OK || status == NG_ERR_OUT_OF_MEMORY);
        if (status == NG_OK) {
            check(same_puzzle(a, puzzle, back));
            for (size_t f = 0; f < NG_NON_FIELD_COUNT; ++f) {
                check(same_text(ng_non_metadata_get(metadata, (ng_non_field)f),
                                ng_non_metadata_get(back_metadata, (ng_non_field)f)));
            }
        }
        ng_puzzle_destroy(back);
        ng_non_metadata_destroy(back_metadata);
    }
    ng_buffer_destroy(text);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    capped_heap heap = {};
    const ng_allocator a = {.alloc = capped_alloc, .free = capped_free, .ctx = &heap};
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, (const char *)data, size);
    ng_puzzle *puzzle = nullptr;
    ng_non_metadata *metadata = nullptr;
    ng_parse_error error;
    memset(&error, 0xA5, sizeof error); /* every byte must be rewritten by the reader */

    const ng_status status = ng_io_read_non(&a, &r, &puzzle, &metadata, &error);

    if (status == NG_OK) {
        check(puzzle != nullptr && metadata != nullptr && error.message[0] == '\0');
        check_round_trip(&a, puzzle, metadata);
    } else {
        check(puzzle == nullptr && metadata == nullptr);
        check(memchr(error.message, '\0', sizeof error.message) != nullptr);
        check(error.line >= 1u && error.column >= 1u && error.message[0] != '\0');
    }
    ng_puzzle_destroy(puzzle);
    ng_non_metadata_destroy(metadata);
    check(heap.live == 0u);
    return 0;
}
