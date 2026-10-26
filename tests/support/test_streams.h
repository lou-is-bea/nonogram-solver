/**
 * @file test_streams.h
 * @brief Test readers and writers: chunked delivery (short reads) and injected I/O failures.
 */
#ifndef NG_TESTS_SUPPORT_TEST_STREAMS_H
#define NG_TESTS_SUPPORT_TEST_STREAMS_H

#include <stddef.h>

#include "nonogram/stream.h"

/** @brief State of a memory reader that returns at most @c chunk bytes per read and fails
 *  with NG_ERR_IO once @c fail_at bytes have been delivered. */
typedef struct ng_test_source {
    const char *data;
    size_t size;
    size_t position;
    size_t chunk;
    size_t fail_at; /* SIZE_MAX: never fails */
} ng_test_source;

/** @brief Reader over @p data delivering at most @p chunk (>= 1) bytes per read. */
ng_reader ng_test_chunked_reader(ng_test_source *source, const char *data, size_t size,
                                 size_t chunk);

/** @brief Reader over @p data that fails with NG_ERR_IO after @p fail_at bytes. */
ng_reader ng_test_failing_reader(ng_test_source *source, const char *data, size_t size,
                                 size_t fail_at);

/** @brief State of a writer that discards its input and fails with NG_ERR_IO once
 *  @c fail_at bytes have been accepted. */
typedef struct ng_test_sink {
    size_t written;
    size_t fail_at;
} ng_test_sink;

/** @brief Writer that fails with NG_ERR_IO on the write that would exceed @p fail_at bytes. */
ng_writer ng_test_failing_writer(ng_test_sink *sink, size_t fail_at);

#endif /* NG_TESTS_SUPPORT_TEST_STREAMS_H */
