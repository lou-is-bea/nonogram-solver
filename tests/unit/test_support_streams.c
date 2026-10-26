#include <criterion/criterion.h>
#include <stddef.h>

#include "nonogram/status.h"
#include "nonogram/stream.h"

#include "ng_test_fixture.h"
#include "test_streams.h"

TestSuite(support_streams, .init = ng_test_setup, .fini = ng_test_teardown);

Test(support_streams, chunked_reader_never_exceeds_its_chunk)
{
    ng_test_source source;
    const ng_reader r = ng_test_chunked_reader(&source, "abcdefg", 7, 3);
    char block[16] = {};
    size_t got = 0;
    const size_t expected[] = {3, 3, 1, 0};

    for (size_t i = 0; i < 4; ++i) {
        cr_assert_eq(r.read(r.ctx, block, sizeof block, &got), NG_OK);
        cr_assert_eq(got, expected[i], "read %zu", i);
    }
}

Test(support_streams, failing_reader_delivers_then_fails)
{
    ng_test_source source;
    const ng_reader r = ng_test_failing_reader(&source, "abcdefg", 7, 5);
    char block[16] = {};
    size_t got = 0;

    cr_assert_eq(r.read(r.ctx, block, sizeof block, &got), NG_OK);
    cr_assert_eq(got, 5u);
    cr_assert_eq(r.read(r.ctx, block, sizeof block, &got), NG_ERR_IO);
    cr_assert_eq(got, 0u);
}

Test(support_streams, failing_writer_accepts_up_to_its_limit)
{
    ng_test_sink sink;
    const ng_writer w = ng_test_failing_writer(&sink, 5);

    cr_assert_eq(w.write(w.ctx, "abc", 3), NG_OK);
    cr_assert_eq(w.write(w.ctx, "de", 2), NG_OK);
    cr_assert_eq(w.write(w.ctx, "f", 1), NG_ERR_IO);
    cr_assert_eq(sink.written, 5u);
}
