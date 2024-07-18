/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Jip J. Dekker <jip.dekker@monash.edu>
 *
 *  Copyright:
 *     Jip J. Dekker, 2024
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

#ifndef __FLATZINC_BLACKBOX_HH__
#define __FLATZINC_BLACKBOX_HH__

#include <cstddef>
#include <string>

#include <gecode/flatzinc.hh>
#include <gecode/kernel.hh>
#ifdef GECODE_HAS_FLOAT_VARS
#include <gecode/float.hh>
#endif

#ifdef _WIN32
#define NOMINMAX // Ensure the words min/max remain available
#include <Windows.h>
#else
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define __stdcall
#endif

namespace Gecode {
namespace FlatZinc {

/// Abstract class implemented by different methods to run blackbox functions
class BlackBoxFn : public SharedHandle::Object {
public:
  virtual void run(const std::vector<int> &int_in,
                   const std::vector<double> &float_in,
                   std::vector<int> &int_out,
                   std::vector<double> &float_out) = 0;
};

/// Implementation of a black box function that dynamically loads a library and
/// run a contained function.
class BlackBoxDLL : public BlackBoxFn {
public:
  BlackBoxDLL(const std::string &name);
  ~BlackBoxDLL();
  void run(const std::vector<int> &int_in, const std::vector<double> &float_in,
           std::vector<int> &int_out, std::vector<double> &float_out) override {
    dll_fzn_blackbox(int_in.data(), int_in.size(), float_in.data(),
                     float_in.size(), int_out.data(), int_out.size(),
                     float_out.data(), float_out.size());
  }

protected:
  void *library;
  void(__stdcall *dll_fzn_blackbox)(const int *, size_t, const double *, size_t,
                                    int *, size_t, double *, size_t);
};

/// Implementation of a black function that starts a seperate process to
/// repeatedly run a blackbox function, communication I/O over pipe.
class BlackBoxExec : public BlackBoxFn {
public:
  BlackBoxExec(const std::string &program);
  ~BlackBoxExec();
  void run(const std::vector<int> &int_in, const std::vector<double> &float_in,
           std::vector<int> &int_out, std::vector<double> &float_out) override;

protected:
#ifdef _WIN32
  HANDLE pipe_send;
  HANDLE pipe_read;
#else
  int pipe_send;
  int pipe_receive;
#endif
};

class BlackBoxHandle : public SharedHandle {
public:
  BlackBoxHandle(BlackBoxFn *fn) : SharedHandle() { object(fn); }
  BlackBoxHandle(const BlackBoxHandle &handle) : SharedHandle(handle) {}
  BlackBoxHandle &operator=(const BlackBoxHandle &handle) {
    return static_cast<BlackBoxHandle &>(SharedHandle::operator=(handle));
  }
  BlackBoxFn *operator()() { return static_cast<BlackBoxFn *>(object()); };
};

class BlackBox : public Propagator {
protected:
  /// Integer variables considered as the integer input to the blackbox function
  ViewArray<Int::IntView> int_input;
  /// Integer variables set to the integer output of the blackbox function
  ViewArray<Int::IntView> int_output;

#ifdef GECODE_HAS_FLOAT_VARS
  /// Integer variables considered as the integer input to the blackbox function
  ViewArray<Float::FloatView> float_input;
  /// Integer variables set to the integer output of the blackbox function
  ViewArray<Float::FloatView> float_output;
#endif

  /// Handle to the implementation of the blackbox function
  ///
  /// The handle ensures that the function implementation can be shared between
  /// copies of the propagator.
  BlackBoxHandle black_box;

  /// Constructor for cloning \a p
  BlackBox(Space &home, BlackBox &p)
      : Propagator(home, p), black_box(p.black_box) {
    int_input.update(home, p.int_input);
    int_output.update(home, p.int_output);
#ifdef GECODE_HAS_FLOAT_VARS
    float_input.update(home, p.float_input);
    float_output.update(home, p.float_output);
#endif
  }

public:
  /// Constructor for creation
  BlackBox(Home home, ViewArray<Int::IntView> &int_in,
           ViewArray<Int::IntView> &int_out,
#ifdef GECODE_HAS_FLOAT_VARS
           ViewArray<Float::FloatView> &float_in,
           ViewArray<Float::FloatView> &float_out,
#endif
           BlackBoxFn *black_box)
      : Propagator(home), int_input(int_in), int_output(int_out),
#ifdef GECODE_HAS_FLOAT_VARS
        float_input(float_in), float_output(float_out),
#endif
        black_box(black_box) {
    int_input.subscribe(home, *this, Int::PC_INT_VAL);
#ifdef GECODE_HAS_FLOAT_VARS
    float_input.subscribe(home, *this, Float::PC_FLOAT_VAL);
#endif
  }
  /// Cost function (defined as low linear)
  PropCost cost(const Space &home, const ModEventDelta &med) const override {
    return PropCost::crazy(PropCost::HI, int_input.size());
  };
  /// Schedule function
  void reschedule(Space &home) override {
    int_input.cancel(home, *this, Int::PC_INT_VAL);
#ifdef GECODE_HAS_FLOAT_VARS
    float_input.cancel(home, *this, Float::PC_FLOAT_VAL);
#endif
  }
  /// Delete propagator and return its size
  size_t dispose(Space &home) override {
    int_input.cancel(home, *this, Int::PC_INT_VAL);
#ifdef GECODE_HAS_FLOAT_VARS
    float_input.cancel(home, *this, Float::PC_FLOAT_VAL);
#endif
    (void)Propagator::dispose(home);
    // destroy plugin container
    return sizeof(*this);
  };

  ExecStatus propagate(Space &home, const ModEventDelta &) override;

  Propagator *copy(Space &home) override {
    return new (home) BlackBox(home, *this);
  }

  static ExecStatus post(Home home, ViewArray<Int::IntView> &int_input,
                         ViewArray<Int::IntView> &int_output,
#ifdef GECODE_HAS_FLOAT_VARS
                         ViewArray<Float::FloatView> &float_input,
                         ViewArray<Float::FloatView> &float_output,
#endif
                         const std::string &mode,
                         const std::string &instantiation) {
    BlackBoxFn *black_box(nullptr);
    if (mode == "dll") {
      black_box = new BlackBoxDLL(instantiation);
    } else if (mode == "exec") {
      black_box = new BlackBoxExec(instantiation);
    } else {
      throw Error("Blackbox", "Unknown blackbox protocol `" + mode + "'");
    }

    new (home) BlackBox(home, int_input, int_output,
#ifdef GECODE_HAS_FLOAT_VARS
                        float_input, float_output,
#endif
                        black_box);
    return ES_OK;
  }
};

void blackbox(Home home, const IntVarArgs &int_in, const IntVarArgs &int_out,
#ifdef GECODE_HAS_FLOAT_VARS
              const FloatVarArgs &float_in, const FloatVarArgs &float_out,
#endif
              const std::string &mode, const std::string &instantiation);

} // namespace FlatZinc
} // namespace Gecode

#endif //__FLATZINC_BLACKBOX_HH__
