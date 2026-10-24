#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

#include "ng_test_fixture.h"
#include "test_allocator.h"

TestSuite(reader_memory, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(reader_file, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(writer_file, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(buffer_create, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(writer_buffer, .init = ng_test_setup, .fini = ng_test_teardown);

static const char sample[] = "width 5\nheight 5\n";
static constexpr size_t sample_size = sizeof sample - 1u;

/* Reads @p r to the end with blocks of at most @p cap bytes; returns the status of the first
 * failing read, NG_OK otherwise. */
static ng_status drain(const ng_reader *r, size_t cap, char *out, size_t out_cap, size_t *total)
{
    *total = 0;
    for (;;) {
        char block[64] = {};
        size_t got = 0;
        const ng_status status = r->read(r->ctx, block, cap, &got);
        if (status != NG_OK) {
            return status;
        }
        if (got == 0u) {
            return NG_OK;
        }
        cr_assert_leq(got, cap);
        cr_assert_leq(*total + got, out_cap);
        memcpy(out + *total, block, got);
        *total += got;
    }
}

static FILE *file_with(const char *content, size_t size)
{
    FILE *file = tmpfile();
    cr_assert_not_null(file);
    cr_assert_eq(fwrite(content, 1, size, file), size);
    cr_assert_eq(fseek(file, 0, SEEK_SET), 0);
    return file;
}

/* --- reader_memory ------------------------------------------------------------------------ */

Test(reader_memory, reads_the_whole_block_in_one_call)
{
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, sample, sample_size);
    char block[64] = {};
    size_t got = 0;

    const ng_status status = r.read(r.ctx, block, sizeof block, &got);

    cr_assert_eq(status, NG_OK);
    cr_assert_eq(got, sample_size);
    cr_assert_arr_eq(block, sample, sample_size);
}

Test(reader_memory, small_blocks_concatenate_to_the_original)
{
    const size_t caps[] = {1, 3, 7, sample_size, 64};

    for (size_t i = 0; i < sizeof caps / sizeof caps[0]; ++i) {
        ng_memory_source source;
        const ng_reader r = ng_reader_from_memory(&source, sample, sample_size);
        char out[64] = {};
        size_t total = 0;

        cr_assert_eq(drain(&r, caps[i], out, sizeof out, &total), NG_OK, "cap %zu", caps[i]);

        cr_assert_eq(total, sample_size, "cap %zu", caps[i]);
        cr_assert_arr_eq(out, sample, sample_size, "cap %zu", caps[i]);
    }
}

Test(reader_memory, end_of_stream_is_repeated)
{
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, "ab", 2);
    char block[8] = {};
    size_t got = 0;
    cr_assert_eq(r.read(r.ctx, block, sizeof block, &got), NG_OK);

    for (size_t i = 0; i < 3; ++i) {
        got = 99;
        cr_assert_eq(r.read(r.ctx, block, sizeof block, &got), NG_OK);
        cr_assert_eq(got, 0u);
    }
}

Test(reader_memory, empty_block_is_immediately_at_end)
{
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, nullptr, 0);
    char block[8] = {};
    size_t got = 99;

    const ng_status status = r.read(r.ctx, block, sizeof block, &got);

    cr_assert_eq(status, NG_OK);
    cr_assert_eq(got, 0u);
}

Test(reader_memory, nullptr_data_with_a_size_is_invalid)
{
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, nullptr, 4);
    char block[8] = {};
    size_t got = 99;

    const ng_status status = r.read(r.ctx, block, sizeof block, &got);

    cr_assert_eq(status, NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(got, 0u);
}

Test(reader_memory, nullptr_source_is_invalid)
{
    const ng_reader r = ng_reader_from_memory(nullptr, sample, sample_size);
    char block[8] = {};
    size_t got = 0;

    cr_assert_eq(r.read(r.ctx, block, sizeof block, &got), NG_ERR_INVALID_ARGUMENT);
}

Test(reader_memory, invalid_read_arguments_are_rejected)
{
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, sample, sample_size);
    char block[8] = {};
    size_t got = 0;

    cr_assert_eq(r.read(r.ctx, nullptr, sizeof block, &got), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(r.read(r.ctx, block, sizeof block, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(r.read(r.ctx, block, 0, &got), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(source.position, 0u, "a rejected read consumes nothing");
}

/* --- reader_file -------------------------------------------------------------------------- */

Test(reader_file, small_blocks_concatenate_to_the_file_content)
{
    FILE *file = file_with(sample, sample_size);
    const ng_reader r = ng_reader_from_file(file);
    char out[64] = {};
    size_t total = 0;

    const ng_status status = drain(&r, 5, out, sizeof out, &total);

    cr_assert_eq(status, NG_OK);
    cr_assert_eq(total, sample_size);
    cr_assert_arr_eq(out, sample, sample_size);
    cr_assert_eq(fclose(file), 0);
}

Test(reader_file, empty_file_is_immediately_at_end)
{
    FILE *file = file_with("", 0);
    const ng_reader r = ng_reader_from_file(file);
    char block[8] = {};
    size_t got = 99;

    const ng_status status = r.read(r.ctx, block, sizeof block, &got);

    cr_assert_eq(status, NG_OK);
    cr_assert_eq(got, 0u);
    cr_assert_eq(fclose(file), 0);
}

Test(reader_file, read_failure_is_an_io_error)
{
    /* Linux: opening a directory for reading succeeds, reading it fails with EISDIR. */
    FILE *dir = fopen(".", "r");
    cr_assert_not_null(dir);
    const ng_reader r = ng_reader_from_file(dir);
    char block[8] = {};
    size_t got = 99;

    const ng_status status = r.read(r.ctx, block, sizeof block, &got);

    cr_assert_eq(status, NG_ERR_IO);
    cr_assert_eq(got, 0u);
    cr_assert_eq(fclose(dir), 0);
}

Test(reader_file, nullptr_file_is_invalid)
{
    const ng_reader r = ng_reader_from_file(nullptr);
    char block[8] = {};
    size_t got = 0;

    cr_assert_eq(r.read(r.ctx, block, sizeof block, &got), NG_ERR_INVALID_ARGUMENT);
}

Test(reader_file, invalid_read_arguments_are_rejected)
{
    FILE *file = file_with(sample, sample_size);
    const ng_reader r = ng_reader_from_file(file);
    char block[8] = {};
    size_t got = 0;

    cr_assert_eq(r.read(r.ctx, nullptr, sizeof block, &got), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(r.read(r.ctx, block, sizeof block, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(r.read(r.ctx, block, 0, &got), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(fclose(file), 0);
}

/* --- writer_file -------------------------------------------------------------------------- */

Test(writer_file, written_bytes_reach_the_file)
{
    FILE *file = tmpfile();
    cr_assert_not_null(file);
    const ng_writer w = ng_writer_from_file(file);

    cr_assert_eq(w.write(w.ctx, "width ", 6), NG_OK);
    cr_assert_eq(w.write(w.ctx, "5\n", 2), NG_OK);

    char back[16] = {};
    cr_assert_eq(fseek(file, 0, SEEK_SET), 0);
    cr_assert_eq(fread(back, 1, sizeof back, file), 8u);
    cr_assert_str_eq(back, "width 5\n");
    cr_assert_eq(fclose(file), 0);
}

Test(writer_file, zero_length_write_is_a_no_op)
{
    FILE *file = tmpfile();
    cr_assert_not_null(file);
    const ng_writer w = ng_writer_from_file(file);

    const ng_status status = w.write(w.ctx, nullptr, 0);

    cr_assert_eq(status, NG_OK);
    cr_assert_eq(ftell(file), 0L);
    cr_assert_eq(fclose(file), 0);
}

Test(writer_file, write_failure_is_an_io_error)
{
    /* Linux: /dev/full fails every write with ENOSPC; unbuffered, the failure is immediate. */
    FILE *full = fopen("/dev/full", "w");
    cr_assert_not_null(full);
    cr_assert_eq(setvbuf(full, nullptr, _IONBF, 0), 0);
    const ng_writer w = ng_writer_from_file(full);

    const ng_status status = w.write(w.ctx, "x", 1);

    cr_assert_eq(status, NG_ERR_IO);
    cr_assert_eq(fclose(full), 0, "unbuffered: nothing left to flush");
}

Test(writer_file, nullptr_file_is_invalid)
{
    const ng_writer w = ng_writer_from_file(nullptr);

    cr_assert_eq(w.write(w.ctx, "x", 1), NG_ERR_INVALID_ARGUMENT);
}

Test(writer_file, nullptr_data_with_a_length_is_invalid)
{
    FILE *file = tmpfile();
    cr_assert_not_null(file);
    const ng_writer w = ng_writer_from_file(file);

    cr_assert_eq(w.write(w.ctx, nullptr, 1), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(fclose(file), 0);
}

/* --- buffer_create ------------------------------------------------------------------------ */

Test(buffer_create, new_buffer_is_empty)
{
    ng_buffer *b = nullptr;

    const ng_status status = ng_buffer_create(&ng_test_heap()->base, &b);

    cr_assert_eq(status, NG_OK);
    cr_assert_not_null(b);
    cr_assert_eq(ng_buffer_size(b), 0u);
    cr_assert_str_eq(ng_buffer_data(b), "");
    ng_buffer_destroy(b);
}

Test(buffer_create, nullptr_arguments_are_invalid)
{
    ng_buffer *b = (ng_buffer *)&b; /* any non-null value: must be reset on failure */

    cr_assert_eq(ng_buffer_create(nullptr, &b), NG_ERR_INVALID_ARGUMENT);
    cr_assert_null(b);
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, nullptr), NG_ERR_INVALID_ARGUMENT);
}

Test(buffer_create, every_allocation_failure_is_clean)
{
    ng_test_allocator *ta = ng_test_heap();
    for (ng_counter n = 0;; ++n) {
        ng_test_allocator_fail_at(ta, n);
        ng_buffer *b = nullptr;

        const ng_status status = ng_buffer_create(&ta->base, &b);

        if (status == NG_OK) {
            ng_buffer_destroy(b);
            break;
        }
        cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
        cr_assert_null(b);
        cr_assert_eq(ta->live_bytes, 0u);
    }
}

Test(buffer_create, destroy_of_nullptr_does_nothing)
{
    ng_buffer_destroy(nullptr);
}

Test(buffer_create, accessors_treat_nullptr_as_empty)
{
    cr_assert_eq(ng_buffer_size(nullptr), 0u);
    cr_assert_str_eq(ng_buffer_data(nullptr), "");
}

/* --- writer_buffer ------------------------------------------------------------------------ */

Test(writer_buffer, writes_accumulate_as_a_c_string)
{
    ng_buffer *b = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &b), NG_OK);
    const ng_writer w = ng_writer_from_buffer(b);

    cr_assert_eq(w.write(w.ctx, "###", 3), NG_OK);
    cr_assert_eq(w.write(w.ctx, "..\n", 3), NG_OK);

    cr_assert_eq(ng_buffer_size(b), 6u);
    cr_assert_str_eq(ng_buffer_data(b), "###..\n");
    ng_buffer_destroy(b);
}

Test(writer_buffer, content_survives_many_growths)
{
    ng_buffer *b = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &b), NG_OK);
    const ng_writer w = ng_writer_from_buffer(b);
    static const char piece[] = "#.#.#.?";
    constexpr size_t piece_size = sizeof piece - 1u;
    constexpr size_t pieces = 20'000;

    for (size_t i = 0; i < pieces; ++i) {
        cr_assert_eq(w.write(w.ctx, piece, piece_size), NG_OK);
    }

    cr_assert_eq(ng_buffer_size(b), pieces * piece_size);
    const char *data = ng_buffer_data(b);
    for (size_t i = 0; i < pieces; ++i) {
        cr_assert_arr_eq(data + (i * piece_size), piece, piece_size, "piece %zu", i);
    }
    cr_assert_eq(data[pieces * piece_size], '\0');
    ng_buffer_destroy(b);
}

Test(writer_buffer, embedded_nul_bytes_are_kept)
{
    ng_buffer *b = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &b), NG_OK);
    const ng_writer w = ng_writer_from_buffer(b);
    static const char binary[] = {'N', 'G', 'G', '\0', 1};

    cr_assert_eq(w.write(w.ctx, binary, sizeof binary), NG_OK);

    cr_assert_eq(ng_buffer_size(b), sizeof binary);
    cr_assert_arr_eq(ng_buffer_data(b), binary, sizeof binary);
    ng_buffer_destroy(b);
}

Test(writer_buffer, failed_growth_leaves_the_content_unchanged)
{
    ng_test_allocator *ta = ng_test_heap();
    ng_buffer *b = nullptr;
    cr_assert_eq(ng_buffer_create(&ta->base, &b), NG_OK);
    const ng_writer w = ng_writer_from_buffer(b);
    cr_assert_eq(w.write(w.ctx, "keep", 4), NG_OK);
    char big[4'096] = {};
    memset(big, '#', sizeof big);
    ng_test_allocator_fail_at(ta, 0);

    const ng_status status = w.write(w.ctx, big, sizeof big);

    cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
    cr_assert_eq(ng_buffer_size(b), 4u);
    cr_assert_str_eq(ng_buffer_data(b), "keep");
    cr_assert_eq(w.write(w.ctx, "!", 1), NG_OK, "the buffer stays usable");
    cr_assert_str_eq(ng_buffer_data(b), "keep!");
    ng_buffer_destroy(b);
}

Test(writer_buffer, every_allocation_failure_during_first_write_is_clean)
{
    ng_test_allocator *ta = ng_test_heap();
    for (ng_counter n = 0;; ++n) {
        ng_buffer *b = nullptr;
        cr_assert_eq(ng_buffer_create(&ta->base, &b), NG_OK);
        const ng_writer w = ng_writer_from_buffer(b);
        ng_test_allocator_fail_at(ta, n);

        const ng_status status = w.write(w.ctx, sample, sample_size);

        if (status == NG_OK) {
            cr_assert_str_eq(ng_buffer_data(b), sample);
            ng_buffer_destroy(b);
            break;
        }
        cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
        cr_assert_eq(ng_buffer_size(b), 0u);
        ng_buffer_destroy(b);
        cr_assert_eq(ta->live_bytes, 0u);
    }
}

Test(writer_buffer, capped_growth_is_out_of_memory)
{
    ng_test_allocator *ta = ng_test_heap();
    ng_buffer *b = nullptr;
    cr_assert_eq(ng_buffer_create(&ta->base, &b), NG_OK);
    const ng_writer w = ng_writer_from_buffer(b);
    ng_test_allocator_cap(ta, ta->live_bytes + 64u);
    char big[256] = {};

    const ng_status status = w.write(w.ctx, big, sizeof big);

    cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
    cr_assert_eq(ng_buffer_size(b), 0u);
    ng_buffer_destroy(b);
}

Test(writer_buffer, unrepresentable_size_is_a_limit_error)
{
    ng_buffer *b = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &b), NG_OK);
    const ng_writer w = ng_writer_from_buffer(b);
    cr_assert_eq(w.write(w.ctx, "x", 1), NG_OK);

    /* The size check precedes any access to the data, so the length is never dereferenced. */
    const ng_status status = w.write(w.ctx, "y", SIZE_MAX);

    cr_assert_eq(status, NG_ERR_LIMIT);
    cr_assert_str_eq(ng_buffer_data(b), "x");
    ng_buffer_destroy(b);
}

Test(writer_buffer, zero_length_write_is_a_no_op)
{
    ng_buffer *b = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &b), NG_OK);
    const ng_writer w = ng_writer_from_buffer(b);

    cr_assert_eq(w.write(w.ctx, nullptr, 0), NG_OK);

    cr_assert_eq(ng_buffer_size(b), 0u);
    ng_buffer_destroy(b);
}

Test(writer_buffer, invalid_arguments_are_rejected)
{
    ng_buffer *b = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &b), NG_OK);
    const ng_writer w = ng_writer_from_buffer(b);
    const ng_writer orphan = ng_writer_from_buffer(nullptr);

    cr_assert_eq(w.write(w.ctx, nullptr, 1), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(orphan.write(orphan.ctx, "x", 1), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_buffer_size(b), 0u);
    ng_buffer_destroy(b);
}
