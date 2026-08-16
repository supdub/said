#pragma once

// Compatibility glue for upstream projects that distinguish only between
// MSVC and Unix even when Clang targets the MinGW ABI through Zig.
#if defined(_WIN32) && !defined(_MSC_VER)
#include <cstdio>

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

inline void said_setlinebuf(FILE* stream) {
    setvbuf(stream, nullptr, _IOLBF, BUFSIZ);
}

#define setlinebuf said_setlinebuf
#define _M_file_ _M_file
#endif
