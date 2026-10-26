#include "nonogram/status.h"

const char *ng_status_str(ng_status status)
{
    switch (status) {
    case NG_OK:
        return "ok";
    case NG_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case NG_ERR_OUT_OF_MEMORY:
        return "out of memory";
    case NG_ERR_LIMIT:
        return "limit exceeded";
    case NG_ERR_PARSE:
        return "parse error";
    case NG_ERR_IO:
        return "input/output error";
    case NG_ERR_INCONSISTENT:
        return "inconsistent puzzle";
    case NG_ERR_NOT_FOUND:
        return "not found";
    case NG_ERR_INTERNAL:
        return "internal error";
    }
    return "unknown status";
}
