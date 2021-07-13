/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *
 *  Copyright:
 *     Jip J. Dekker, 2018
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

#include "complete.hh"

namespace Gecode { namespace FlatZinc {


  Complete::Complete(Space &home, Complete &p)
  : UnaryPropagator<BoolView, PC_BOOL_VAL>(home,p), c(p.c) {}

  Complete::Complete(Home home, BoolView x0, std::shared_ptr<bool> c)
      : c(c), UnaryPropagator<BoolView, PC_BOOL_VAL>(home, x0) {}

  Actor* Complete::copy(Space &home) {
    return new (home) Complete(home,*this);
  }

  PropCost Complete::cost(const Space &home, const ModEventDelta &med) const {
    return PropCost::record();
  }

  ExecStatus Complete::propagate(Space &home, const ModEventDelta &med) {
    assert(x0.assigned());
    (*c) = x0.val();
    return home.ES_SUBSUMED(*this);
  }

  ExecStatus Complete::post(Home home, BoolView x0, std::shared_ptr<bool> c) {
		assert(c != nullptr);
    if (x0.assigned()) {
      (*c) = x0.val();
    } else {
      (void) new (home) Complete(home, x0, c);
    }
    return ES_OK;
  }


}}
