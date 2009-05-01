/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Christian Schulte <schulte@gecode.org>
 *
 *  Contributing authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Christian Schulte, 2004
 *     Guido Tack, 2004
 *
 *  Last modified:
 *     $Date: 2009-01-21 22:10:32 +0100 (Wed, 21 Jan 2009) $ by $Author: schulte $
 *     $Revision: 8093 $
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

#include <gecode/search.hh>

namespace Gecode { namespace Search {
    
  namespace Sequential {

    /// Create branch and bound engine
    GECODE_SEARCH_EXPORT Engine* bab(Space* s, size_t sz, const Options& o);

  }

#ifdef GECODE_HAS_THREADS

  namespace Parallel {

    /// Create branch and bound engine
    GECODE_SEARCH_EXPORT Engine* bab(Space* s, size_t sz, const Options& o);

  }

#endif

  Engine* 
  bab(Space* s, size_t sz, const Options& o) {
#ifdef GECODE_HAS_THREADS
    Options to = threads(o);
    if (to.threads == 1)
      return Sequential::bab(s,sz,to);
    else
      return Parallel::bab(s,sz,to);
#else
    return Sequential::bab(s,sz,o);
#endif
  }

}}

// STATISTICS: search-any