
//          Copyright Alexander Bulovyatov 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file ../../LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#ifndef INDEXED_DEBUG
#ifdef NDEBUG
#define INDEXED_DEBUG 0
#else
#define INDEXED_DEBUG 1
#endif
#endif

#if INDEXED_DEBUG == 1
#include <cstdio>
#define indexed_warning(arg) if (!(arg)) std::fprintf(stderr, "Warning `%s' triggered at %s:%d\n", \
#arg, __FILE__, __LINE__); else ((void)0)
#ifdef NDEBUG
#include <cstdlib>
#define indexed_assert(arg) if (!(arg)) { std::fprintf(stderr, "Assertion `%s' failed at %s:%d\n", \
#arg, __FILE__, __LINE__); std::abort(); } else ((void)0)
#else
#include <cassert>
#define indexed_assert(arg) assert(arg)
#endif
#else
#define indexed_assert(arg) ((void)0)
#define indexed_warning(arg) ((void)0)
#endif
