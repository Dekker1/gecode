/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/// Concatenate \a R and \a LINE
#define GECODE_CAT2(R,LINE) R ## LINE
/// Concatenate macro-expanded \a R and \a LINE
#define GECODE_CAT(R,LINE) GECODE_CAT2(R,LINE)
/// Generate fresh name with prefix P
#define GECODE_FRESH(P) GECODE_CAT(GECODE_ ## P, __LINE__)

/**
 * \def GECODE_NEVER
 * \brief Assert that this command is never executed
 *
 * This is preferred over assert(false) as it is used for optimization,
 * if supported by a compiler (for example, Microsoft Visual C++).
 *
 */

#if defined(_MSC_VER) && defined(NDEBUG)

#define GECODE_NEVER __assume(false)

#else

#define GECODE_NEVER assert(false)

#endif

/**
 * \def GECODE_NOT_NULL
 * \brief Assert that a pointer is never nullptr
 *
 * This is preferred over assert as it is used for optimization,
 * if supported by a compiler (for example, Microsoft Visual C++).
 *
 */

#if defined(_MSC_VER) && defined(NDEBUG)

#define GECODE_NOT_NULL(p) __assume(p != nullptr)

#else

#define GECODE_NOT_NULL(p) assert(p != nullptr)

#endif

/**
 * \def GECODE_VALID_FUNCTION
 * \brief Assert that a function is valid
 *
 * This is preferred over assert as it is used for optimization,
 * if supported by a compiler (for example, Microsoft Visual C++).
 *
 */

#if defined(_MSC_VER) && defined(NDEBUG)

#define GECODE_VALID_FUNCTION(f) __assume(static_cast<bool>(f))

#else

#define GECODE_VALID_FUNCTION(f) assert(static_cast<bool>(f))

#endif

/**
 * \def GECODE_ASSUME
 * \brief Assert certain property
 *
 * This might be used for optimization as well (for example,
 * Microsoft Visual C++), otherwise it behaves like any
 * assert.
 *
 */

#if defined(_MSC_VER) && defined(NDEBUG)

#define GECODE_ASSUME(p) __assume((p))

#else

#define GECODE_ASSUME(p) assert((p))

#endif

// STATISTICS: support-any
