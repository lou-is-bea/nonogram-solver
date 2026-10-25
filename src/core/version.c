#include "nonogram/version.h"

/* NG_VERSION_STRING is injected by CMake from the same project() version as the constexpr
 * components of version.h; test_version checks that both agree. */
const char *ng_version_string(void)
{
    return NG_VERSION_STRING;
}
