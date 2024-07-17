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
#include <dlfcn.h>
// NOLINTNEXTLINE(bugprone-reserved-identifier)
#define __stdcall
#endif

namespace Gecode {
namespace FlatZinc {

/// Abstract class implemented by different methods to run blackbox functions
class BlackBoxFn : public SharedHandle::Object {
public:
  virtual void run(const int *int_in, size_t int_in_len, const double *float_in,
                   size_t float_in_len, int *int_out, size_t int_out_len,
                   double *float_out, size_t float_out_len) = 0;
};

/// Implementation of a black box function that dynamically loads a library and
/// run a contained function.
class BlackBoxDLL : public BlackBoxFn {
public:
  BlackBoxDLL(const std::string &name) {
    std::string loadError;
#ifdef _WIN32
    library = LoadLibrary(name.c_str());
    if (!library) {
      loadError = std::string("unable to locate library `") + name + "'";
      library = LoadLibrary((std::string(name) + ".dll").c_str());
    }
    if (!library) {
      library = LoadLibrary((std::string("lib") + name + ".dll").c_str());
    }
#else
    library = dlopen(name.c_str(), RTLD_LAZY);
    if (!library) {
      loadError = std::string(dlerror());
      library = dlopen((name + ".so").c_str(), RTLD_NOW);
    }
    if (!library) {
      library = dlopen((std::string("lib") + name + ".so").c_str(), RTLD_NOW);
    }
#endif
    if (!library) {
      throw Error("Blackbox", "Unable to open dynamic library: " + loadError);
    }

    // find symbol for blacbox function
#ifdef _WIN32
    *(void **)(&dll_fzn_blackbox) =
        GetProcAddress((HMODULE)library, "fzn_blackbox");
    std::string symError = ".";
#else
    *(void **)(&dll_fzn_blackbox) = dlsym(library, "fzn_blackbox");
    std::string symError(": ");
    if (!dll_fzn_blackbox) {
      symError += std::string(dlerror());
    }
#endif
    if (!dll_fzn_blackbox) {
      throw Error("Blackbox",
                  "Unable to find symbol `fzn_blackbox` in dynamic library" +
                      symError);
    }
  }
  ~BlackBoxDLL() {
    if (library) {
#ifdef _WIN32
      FreeLibrary((HMODULE)library);
#else
      dlclose(library);
#endif
    }
  }
  void run(const int *int_in, size_t int_in_len, const double *float_in,
           size_t float_in_len, int *int_out, size_t int_out_len,
           double *float_out, size_t float_out_len) override {
    dll_fzn_blackbox(int_in, int_in_len, float_in, float_in_len, int_out,
                     int_out_len, float_out, float_out_len);
  }

private:
  void *library;
  void(__stdcall *dll_fzn_blackbox)(const int *, size_t, const double *, size_t,
                                    int *, size_t, double *, size_t);
};

/// Implementation of a black function that starts a seperate process to
/// repeatedly run a blackbox function, communication I/O over pipe.
class BlackBoxExec : BlackBoxFn {
public:
  BlackBoxExec(const std::string &cmd);
  ~BlackBoxExec();
  void run(const int *int_in, size_t int_in_len, const double *float_in,
           size_t float_in_len, int *int_out, size_t int_out_len,
           double *float_out, size_t float_out_len) override;
};

class BlackBoxHandle : public SharedHandle {
public:
  BlackBoxHandle(BlackBoxFn *fn) : SharedHandle() { object(fn); }
  BlackBoxHandle(const BlackBoxHandle &handle) : SharedHandle(handle) {}
  BlackBoxHandle &operator=(const BlackBoxHandle &handle) {
    return static_cast<BlackBoxHandle &>(SharedHandle::operator=(handle));
  }
  virtual ~BlackBoxHandle(){};
  BlackBoxFn *operator()() { return static_cast<BlackBoxFn *>(object()); };
};

class BlackBox : public Propagator {
protected:
  /// Integer variables considered as the integer input to the blackbox function
  ViewArray<Int::IntView> int_input;
  /// Integer variables set to the integer output of the blackbox function
  ViewArray<Int::IntView> int_output;
  /// Integer variables considered as the integer input to the blackbox function
  ViewArray<Float::FloatView> float_input;
  /// Integer variables set to the integer output of the blackbox function
  ViewArray<Float::FloatView> float_output;

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
  virtual PropCost cost(const Space &home, const ModEventDelta &med) const {
    return PropCost::crazy(PropCost::HI, int_input.size());
  };
  /// Schedule function
  virtual void reschedule(Space &home) {
    int_input.cancel(home, *this, Int::PC_INT_VAL);
#ifdef GECODE_HAS_FLOAT_VARS
    float_input.cancel(home, *this, Float::PC_FLOAT_VAL);
#endif
  }
  /// Delete propagator and return its size
  virtual size_t dispose(Space &home) {
    int_input.cancel(home, *this, Int::PC_INT_VAL);
#ifdef GECODE_HAS_FLOAT_VARS
    float_input.cancel(home, *this, Float::PC_FLOAT_VAL);
#endif
    (void)Propagator::dispose(home);
    // destroy plugin container
    return sizeof(*this);
  };

  virtual ExecStatus propagate(Space &home, const ModEventDelta &) {
    if (int_input.assigned()
#ifdef GECODE_HAS_FLOAT_VARS
        && float_input.assigned()
#endif
    ) {
      std::vector<int> int_in(int_input.size());
      std::vector<int> int_out(int_output.size());
      // std::cerr << "Black Box Fn input: ";
      for (int i = 0; i < int_in.size(); i++) {
        // std::cerr << int_input[i].val() << " ";
        int_in[i] = int_input[i].val();
      }
#ifdef GECODE_HAS_FLOAT_VARS
      std::vector<double> float_in(float_input.size());
      std::vector<double> float_out(float_output.size());
      for (int i = 0; i < float_in.size(); i++) {
        // std::cerr << float_in[i].val() << " ";
        float_in[i] = float_input[i].val().med();
      }
      const double *float_in_ptr = float_in.data();
      size_t float_in_size = float_in.size();
      double *float_out_ptr = float_out.data();
      size_t float_out_size = float_out.size();
#else
      const double *float_in_ptr = nullptr;
      size_t float_in_size = 0;
      const double *float_out_ptr = nullptr;
      size_t float_out_size = 0;
#endif
      // std::cerr << std::endl;

      black_box()->run(int_in.data(), int_in.size(), float_in_ptr,
                       float_in_size, int_out.data(), int_output.size(),
                       float_out_ptr, float_out_size);

      // std::cerr << "Black Box Fn output: ";
      for (int i = 0; i < int_out.size(); i++) {
        // std::cerr << out[i] << " ";
        GECODE_ME_CHECK(int_output[i].eq(home, int_out[i]));
      }
      // std::cerr << std::endl;

      return home.ES_SUBSUMED(*this);
    }
    return ES_FIX;
  }

  virtual Propagator *copy(Space &home) {
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

inline void
blackbox(Home home, const IntVarArgs &int_in, const IntVarArgs &int_out,
#ifdef GECODE_HAS_FLOAT_VARS
         const FloatVarArgs &float_in, const FloatVarArgs &float_out,
#endif
         const std::string &mode, const std::string &instantiation) {
  ViewArray<Int::IntView> int_input(home, int_in);
  ViewArray<Int::IntView> int_output(home, int_out);
#ifdef GECODE_HAS_FLOAT_VARS
  ViewArray<Float::FloatView> float_input(home, float_in);
  ViewArray<Float::FloatView> float_output(home, float_out);
#endif

  if (home.failed())
    return;
  PostInfo pi(home);
  ExecStatus es = BlackBox::post(home, int_input, int_output,
#ifdef GECODE_HAS_FLOAT_VARS
                                 float_input, float_output,
#endif
                                 mode, instantiation);
  GECODE_ES_FAIL(es);
}

} // namespace FlatZinc
} // namespace Gecode

#endif //__FLATZINC_BLACKBOX_HH__
