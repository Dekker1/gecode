/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Gecode behind the FZnSO solver protocol: see `fznso.hh`.
 */

#include <gecode/flatzinc/fznso.hh>
#include <gecode/flatzinc/complete.hh>
#include <gecode/flatzinc/lastval.hh>
#include <gecode/flatzinc/registry.hh>

#include <gecode/driver.hh>
#include <gecode/search.hh>

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace Gecode { namespace FlatZinc {

  // -------------------------------------------------------------------------
  // FznsoSpace
  // -------------------------------------------------------------------------

  namespace {

    /// Append `add` to `arr`, keeping the variables already in it.
    ///
    /// `FlatZincSpace::init` sizes the arrays once and for all, which a layered
    /// model outgrows. Rebuilding the array from an argument list copies
    /// variable *handles* only, so nothing is re-created.
    template<class Array, class Args>
    void appendVars(Space& home, Array& arr, const Args& add) {
      if (add.size() == 0)
        return;
      Args all(arr.size()+add.size());
      for (int i=arr.size(); i--; )
        all[i] = arr[i];
      for (int i=add.size(); i--; )
        all[arr.size()+i] = add[i];
      arr = Array(home, all);
    }

    /// Record the `introduced` / `funcDep` pair `FlatZincSpace` keeps for each
    /// variable, in the interleaved layout `createBranchers` reads.
    void appendFlags(std::vector<bool>& flags, const std::vector<char>& introduced,
                     const std::vector<char>& funcDep) {
      for (std::size_t i=0; i<introduced.size(); i++) {
        flags.push_back(introduced[i] != 0);
        flags.push_back(funcDep[i] != 0);
      }
    }

  }

  void
  FznsoSpace::addIntVars(const IntVarArgs& add, const std::vector<char>& introduced,
                         const std::vector<char>& funcDep) {
    appendVars(*this, iv, add);
    appendFlags(iv_introduced, introduced, funcDep);
    // `arg2boolvarargs` reads the alias of every integer variable, so the array
    // has to keep pace with `iv`.
    int* alias = alloc<int>(iv.size()+(iv.size()==0?1:0));
    for (int i=iv.size(); i--; )
      alias[i] = (iv_boolalias != nullptr && i < intVarCount) ? iv_boolalias[i] : -1;
    iv_boolalias = alias;
    intVarCount = iv.size();
  }

  void
  FznsoSpace::addBoolVars(const BoolVarArgs& add, const std::vector<char>& introduced,
                          const std::vector<char>& funcDep) {
    appendVars(*this, bv, add);
    appendFlags(bv_introduced, introduced, funcDep);
    boolVarCount = bv.size();
  }

#ifdef GECODE_HAS_SET_VARS
  void
  FznsoSpace::addSetVars(const SetVarArgs& add, const std::vector<char>& introduced,
                         const std::vector<char>& funcDep) {
    appendVars(*this, sv, add);
    appendFlags(sv_introduced, introduced, funcDep);
    setVarCount = sv.size();
  }
#endif

#ifdef GECODE_HAS_FLOAT_VARS
  void
  FznsoSpace::addFloatVars(const FloatVarArgs& add, const std::vector<char>& introduced,
                           const std::vector<char>& funcDep) {
    appendVars(*this, fv, add);
    appendFlags(fv_introduced, introduced, funcDep);
    floatVarCount = fv.size();
  }
#endif

  // -------------------------------------------------------------------------
  // Options
  // -------------------------------------------------------------------------

  namespace {

    /// The `FlatZincOptions` `createBranchers` expects, with the setters it
    /// does not otherwise expose.
    class Options : public FlatZincOptions {
    public:
      Options(void) : FlatZincOptions("gecode") {
        // Nothing here owns a terminal, and the run is driven by a caller that
        // has its own idea of cancellation.
        _interrupt.value(false);
      }

      // Each setter would otherwise hide the base class getter of the same
      // name, which `createBranchers` and the search set-up both call.
      using FlatZincOptions::a_d;
      using FlatZincOptions::c_d;
      using FlatZincOptions::decay;
      using FlatZincOptions::fail;
      using FlatZincOptions::node;
      using FlatZincOptions::nogoods;
      using FlatZincOptions::nogoods_limit;
      using FlatZincOptions::restart;
      using FlatZincOptions::restart_base;
      using FlatZincOptions::restart_limit;
      using FlatZincOptions::restart_scale;
      using FlatZincOptions::seed;
      using FlatZincOptions::step;
      using FlatZincOptions::threads;
      using FlatZincOptions::time;

      void seed(int v) { _seed.value(v); }
      void decay(double v) { _decay.value(v); }
      void threads(double v) { _threads.value(v); }
      void time(double v) { _time.value(v); }
      void node(unsigned long long int v) { _node.value(v); }
      void fail(unsigned long long int v) { _fail.value(v); }
      void c_d(unsigned int v) { _c_d.value(v); }
      void a_d(unsigned int v) { _a_d.value(v); }
      void nogoods(bool v) { _nogoods.value(v); }
      void nogoods_limit(unsigned int v) { _nogoods_limit.value(v); }
      void restart_limit(unsigned long long int v) { _r_limit.value(v); }
      void step(double v) { _step.value(v); }
    };

    /// The option values a consumer sets, kept alongside the Gecode options
    /// because several have no `FlatZincOptions` counterpart.
    struct Settings {
      bool all_solutions = false;
      bool fixed_search = false;
      bool intermediate = false;
      bool verbose = false;
      std::int64_t threads = 1;
      std::int64_t solution_limit = 0;
      bool has_seed = false;
      std::int64_t seed = 0;
      bool has_time_limit = false;
      std::int64_t time_limit = 0;
      bool has_node_limit = false;
      std::int64_t node_limit = 0;
      bool has_fail_limit = false;
      std::int64_t fail_limit = 0;
      bool has_restart_limit = false;
      std::int64_t restart_limit = 0;
      std::string restart = "none";
      double restart_base = 1.5;
      std::int64_t restart_scale = 250;
      bool nogoods = false;
      std::int64_t nogoods_limit = Search::Config::nogoods_limit;
      std::int64_t c_d = Search::Config::c_d;
      std::int64_t a_d = Search::Config::a_d;
      double decay = 0.99;
      double step = 0.0;
    };

    RestartMode restartMode(const std::string& name, bool& known) {
      known = true;
      if (name == "none") return RM_NONE;
      if (name == "constant") return RM_CONSTANT;
      if (name == "linear") return RM_LINEAR;
      if (name == "luby") return RM_LUBY;
      if (name == "geometric") return RM_GEOMETRIC;
      known = false;
      return RM_NONE;
    }

  }

  // -------------------------------------------------------------------------
  // Model translation
  // -------------------------------------------------------------------------

  namespace {

    [[noreturn]] void fail(const std::string& what) {
      throw FlatZinc::Error("FZnSO", what);
    }

    /// Narrow a protocol integer to Gecode's, refusing anything it cannot hold.
    int toInt(std::int64_t v) {
      if (v < Int::Limits::min || v > Int::Limits::max)
        fail("integer " + std::to_string(v) + " is outside Gecode's integer range");
      return static_cast<int>(v);
    }

    /// Narrow a domain bound, which may legitimately sit outside Gecode's range
    /// and is then simply the widest domain Gecode can express.
    int clampBound(std::int64_t v) {
      return static_cast<int>(std::max<std::int64_t>(
        Int::Limits::min, std::min<std::int64_t>(Int::Limits::max, v)));
    }

    /// The inclusive ranges of an integer-set value, clamped to Gecode's range.
    std::vector<int> rangePairs(const fznso::Value& v) {
      std::vector<int> pairs;
      for (std::size_t i=0; i<v.size(); i++) {
        fznso::Range<std::int64_t> r = v.int_range(i);
        if (r.max < Int::Limits::min || r.min > Int::Limits::max)
          continue;
        pairs.push_back(clampBound(r.min));
        pairs.push_back(clampBound(r.max));
      }
      return pairs;
    }

    IntSet toIntSet(const fznso::Value& v) {
      std::vector<int> pairs = rangePairs(v);
      if (pairs.empty())
        return IntSet::empty;
      return IntSet(reinterpret_cast<const int (*)[2]>(pairs.data()),
                    static_cast<int>(pairs.size()/2));
    }

    /// An integer set as a %FlatZinc AST literal.
    ///
    /// `AST::SetLit` holds either one interval or an explicit list of values, so
    /// a domain of several ranges has to be spelled out.
    AST::SetLit* toSetLit(const fznso::Value& v) {
      std::vector<int> pairs = rangePairs(v);
      if (pairs.empty())
        return new AST::SetLit(1, 0);
      if (pairs.size() == 2)
        return new AST::SetLit(pairs[0], pairs[1]);
      // ponytail: a multi-range set is expanded value by value, which is what
      // `AST::SetLit` can carry; switch to a sparse representation if a real
      // model ever hands over a set this large.
      long long total = 0;
      for (std::size_t i=0; i<pairs.size(); i+=2)
        total += static_cast<long long>(pairs[i+1]) - pairs[i] + 1;
      if (total > 10000000LL)
        fail("integer set of " + std::to_string(total) +
             " values is too large to pass to Gecode");
      std::vector<int> values;
      values.reserve(static_cast<std::size_t>(total));
      for (std::size_t i=0; i<pairs.size(); i+=2)
        for (int x=pairs[i]; x<=pairs[i+1]; x++)
          values.push_back(x);
      return new AST::SetLit(values);
    }

    /// The `on_restart` constraints a model has posted so far.
    ///
    /// Gecode implements these by *recording* variables rather than by posting a
    /// propagator: the arrays it reads them from are laid out group by group and
    /// sized from the totals, so they can only be built once every constraint is
    /// known. They are collected here as indices into the space's own arrays,
    /// and materialised onto the space a run searches.
    struct OnRestart {
      /// `{ input, output }` pairs, by variable kind.
      std::vector<std::array<int,2> > solInt, lastValInt, solBool, lastValBool;
      /// `{ low, high, output }`.
      std::vector<std::array<int,3> > uniformInt;
      /// The variable the restart status is written to, or -1.
      int statusIdx = -1;
      /// The variable that marks the search complete, or -1.
      int completeIdx = -1;
#ifdef GECODE_HAS_SET_VARS
      std::vector<std::array<int,2> > solSet, lastValSet;
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      std::vector<std::array<int,2> > solFloat, lastValFloat;
      struct UniformFloat { FloatNum low, high; int out; };
      std::vector<UniformFloat> uniformFloat;
#endif

      bool empty() const {
        return solInt.empty() && lastValInt.empty() && uniformInt.empty() &&
               solBool.empty() && lastValBool.empty() && statusIdx < 0 && completeIdx < 0
#ifdef GECODE_HAS_SET_VARS
               && solSet.empty() && lastValSet.empty()
#endif
#ifdef GECODE_HAS_FLOAT_VARS
               && solFloat.empty() && lastValFloat.empty() && uniformFloat.empty()
#endif
          ;
      }
      void clear() { *this = OnRestart(); }
    };

    /// Build the arrays `on_restart` reads onto \a space, in the layout the
    /// search expects: for each kind, the `sol` inputs, then the `sol` outputs,
    /// then the `last_val` outputs, then the `uniform` outputs, then the status.
    void applyOnRestart(FznsoSpace& space, const OnRestart& r) {
      if (r.empty())
        return;
      space.restart_data.init();

      int ivSize = static_cast<int>(r.solInt.size() * 2 + r.lastValInt.size() +
                                    r.uniformInt.size() + (r.statusIdx >= 0 ? 1 : 0));
      if (ivSize > 0) {
        space.on_restart_iv = IntVarArray(space, ivSize);
        int base = 0;
        space.restart_data().on_restart_iv_sol = static_cast<int>(r.solInt.size());
        for (std::size_t i=0; i<r.solInt.size(); i++) {
          space.on_restart_iv[base+static_cast<int>(i)] = space.iv[r.solInt[i][0]];
          space.on_restart_iv[base+static_cast<int>(r.solInt.size()+i)] = space.iv[r.solInt[i][1]];
        }
        base += static_cast<int>(r.solInt.size()) * 2;

        space.restart_data().last_val_int = std::vector<int>(r.lastValInt.size());
        IntVarArgs lastVal;
        for (std::size_t i=0; i<r.lastValInt.size(); i++) {
          space.on_restart_iv[base+static_cast<int>(i)] = space.iv[r.lastValInt[i][1]];
          lastVal << space.iv[r.lastValInt[i][0]];
        }
        LastValInt::post(space, lastVal);
        base += static_cast<int>(r.lastValInt.size());

        space.restart_data().uniform_range_int =
          std::vector<std::pair<int,int> >(r.uniformInt.size());
        for (std::size_t i=0; i<r.uniformInt.size(); i++) {
          space.restart_data().uniform_range_int[i] =
            std::pair<int,int>(r.uniformInt[i][0], r.uniformInt[i][1]);
          space.on_restart_iv[base+static_cast<int>(i)] = space.iv[r.uniformInt[i][2]];
        }
        base += static_cast<int>(r.uniformInt.size());

        if (r.statusIdx >= 0) {
          space.restart_data().on_restart_status = true;
          space.on_restart_iv[base] = space.iv[r.statusIdx];
        }
      }

      int bvSize = static_cast<int>(r.solBool.size() * 2 + r.lastValBool.size());
      if (bvSize > 0) {
        space.on_restart_bv = BoolVarArray(space, bvSize);
        int base = 0;
        space.restart_data().on_restart_bv_sol = static_cast<int>(r.solBool.size());
        for (std::size_t i=0; i<r.solBool.size(); i++) {
          space.on_restart_bv[base+static_cast<int>(i)] = space.bv[r.solBool[i][0]];
          space.on_restart_bv[base+static_cast<int>(r.solBool.size()+i)] = space.bv[r.solBool[i][1]];
        }
        base += static_cast<int>(r.solBool.size()) * 2;

        space.restart_data().last_val_bool = std::vector<bool>(r.lastValBool.size());
        BoolVarArgs lastVal;
        for (std::size_t i=0; i<r.lastValBool.size(); i++) {
          space.on_restart_bv[base+static_cast<int>(i)] = space.bv[r.lastValBool[i][1]];
          lastVal << space.bv[r.lastValBool[i][0]];
        }
        LastValBool::post(space, lastVal);
      }
      if (r.completeIdx >= 0)
        Complete::post(space, space.bv[r.completeIdx]);

#ifdef GECODE_HAS_SET_VARS
      int svSize = static_cast<int>(r.solSet.size() * 2 + r.lastValSet.size());
      if (svSize > 0) {
        space.on_restart_sv = SetVarArray(space, svSize);
        int base = 0;
        space.restart_data().on_restart_sv_sol = static_cast<int>(r.solSet.size());
        for (std::size_t i=0; i<r.solSet.size(); i++) {
          space.on_restart_sv[base+static_cast<int>(i)] = space.sv[r.solSet[i][0]];
          space.on_restart_sv[base+static_cast<int>(r.solSet.size()+i)] = space.sv[r.solSet[i][1]];
        }
        base += static_cast<int>(r.solSet.size()) * 2;

        space.restart_data().last_val_set = std::vector<IntSet>(r.lastValSet.size());
        SetVarArgs lastVal;
        for (std::size_t i=0; i<r.lastValSet.size(); i++) {
          space.on_restart_sv[base+static_cast<int>(i)] = space.sv[r.lastValSet[i][1]];
          lastVal << space.sv[r.lastValSet[i][0]];
        }
        LastValSet::post(space, lastVal);
      }
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      int fvSize = static_cast<int>(r.solFloat.size() * 2 + r.lastValFloat.size() +
                                    r.uniformFloat.size());
      if (fvSize > 0) {
        space.on_restart_fv = FloatVarArray(space, fvSize);
        int base = 0;
        space.restart_data().on_restart_fv_sol = static_cast<int>(r.solFloat.size());
        for (std::size_t i=0; i<r.solFloat.size(); i++) {
          space.on_restart_fv[base+static_cast<int>(i)] = space.fv[r.solFloat[i][0]];
          space.on_restart_fv[base+static_cast<int>(r.solFloat.size()+i)] =
            space.fv[r.solFloat[i][1]];
        }
        base += static_cast<int>(r.solFloat.size()) * 2;

        space.restart_data().last_val_float = std::vector<FloatVal>(r.lastValFloat.size());
        FloatVarArgs lastVal;
        for (std::size_t i=0; i<r.lastValFloat.size(); i++) {
          space.on_restart_fv[base+static_cast<int>(i)] = space.fv[r.lastValFloat[i][1]];
          lastVal << space.fv[r.lastValFloat[i][0]];
        }
        LastValFloat::post(space, lastVal);
        base += static_cast<int>(r.lastValFloat.size());

        space.restart_data().uniform_range_float =
          std::vector<std::pair<FloatVal,FloatVal> >(r.uniformFloat.size());
        for (std::size_t i=0; i<r.uniformFloat.size(); i++) {
          space.restart_data().uniform_range_float[i] =
            std::pair<FloatVal,FloatVal>(r.uniformFloat[i].low, r.uniformFloat[i].high);
          space.on_restart_fv[base+static_cast<int>(i)] = space.fv[r.uniformFloat[i].out];
        }
      }
#endif
    }

    /// Recovers each decision variable's type and turns model layers into
    /// Gecode variables and constraints.
    class Translator {
    public:
      Translator(const fznso::Model& model, std::vector<VarRef>& vars, OnRestart& onRestart)
        : model(model), vars(vars), onRestart(onRestart) {}

      /// Record which Gecode array the decisions in `[from, to)` belong in.
      void readTypes(std::size_t from, std::size_t to);
      /// Create the decisions in `[from, to)` in `space`.
      void addVars(FznsoSpace& space, std::size_t from, std::size_t to);
      /// Post the constraints in `[from, to)` into `space`.
      void postConstraints(FznsoSpace& space, std::size_t from, std::size_t to);

      /// The objective annotations, as the AST `createBranchers` reads.
      AST::Array* solveAnnotations() const;

      /// The node for a value at an argument position of type `expected`.
      AST::Node* node(const fznso::Value& value, const FznsoType& expected) const;

    private:
      /// Which of Gecode's variable arrays a declared type belongs in.
      static VarKind kindOf(const FznsoType& t, std::size_t decision);

      AST::Node* varNode(fznso::Decision d) const;
      AST::Node* annotationNode(const fznso::AnnotationRef& ann) const;

      /// Record \a con if it is one of the `on_restart` family, which is
      /// collected rather than posted. Returns whether it was.
      bool collectOnRestart(const std::string& ident, fznso::Constraint con);
      /// The index of argument \a a of \a con in the space's own array.
      int argIndex(fznso::Constraint con, std::size_t a) const;

      const fznso::Model& model;
      std::vector<VarRef>& vars;
      OnRestart& onRestart;
    };

    int Translator::argIndex(fznso::Constraint con, std::size_t a) const {
      fznso::Value v = model.constraint_argument(con, a);
      if (v.kind() != FznsoValueDecision)
        fail("an `on_restart` argument must be a decision variable");
      fznso::Decision d = v.as_decision();
      if (d.index >= vars.size() || vars[d.index].index < 0)
        fail("`on_restart` refers to a decision variable that is not in the model");
      return vars[d.index].index;
    }

    bool Translator::collectOnRestart(const std::string& ident, fznso::Constraint con) {
      if (ident.compare(0, 11, "on_restart_") != 0)
        return false;
      const std::string what = ident.substr(11);
      if (what == "status") {
        onRestart.statusIdx = argIndex(con, 0);
      } else if (what == "complete") {
        onRestart.completeIdx = argIndex(con, 0);
      } else if (what == "sol_int") {
        onRestart.solInt.push_back({argIndex(con,0), argIndex(con,1)});
      } else if (what == "last_val_int") {
        onRestart.lastValInt.push_back({argIndex(con,0), argIndex(con,1)});
      } else if (what == "sol_bool") {
        onRestart.solBool.push_back({argIndex(con,0), argIndex(con,1)});
      } else if (what == "last_val_bool") {
        onRestart.lastValBool.push_back({argIndex(con,0), argIndex(con,1)});
      } else if (what == "uniform_int") {
        std::int64_t low = model.constraint_argument(con, 0).as_int();
        std::int64_t high = model.constraint_argument(con, 1).as_int();
        if (low > high)
          fail("`on_restart_uniform_int` needs low <= high");
        onRestart.uniformInt.push_back(
          {static_cast<int>(low), static_cast<int>(high), argIndex(con, 2)});
#ifdef GECODE_HAS_SET_VARS
      } else if (what == "sol_set") {
        onRestart.solSet.push_back({argIndex(con,0), argIndex(con,1)});
      } else if (what == "last_val_set") {
        onRestart.lastValSet.push_back({argIndex(con,0), argIndex(con,1)});
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      } else if (what == "sol_float") {
        onRestart.solFloat.push_back({argIndex(con,0), argIndex(con,1)});
      } else if (what == "last_val_float") {
        onRestart.lastValFloat.push_back({argIndex(con,0), argIndex(con,1)});
      } else if (what == "uniform_float") {
        OnRestart::UniformFloat u;
        u.low = model.constraint_argument(con, 0).as_float();
        u.high = model.constraint_argument(con, 1).as_float();
        if (u.low > u.high)
          fail("`on_restart_uniform_float` needs low <= high");
        u.out = argIndex(con, 2);
        onRestart.uniformFloat.push_back(u);
#endif
      } else {
        return false;
      }
      return true;
    }

    VarKind Translator::kindOf(const FznsoType& t, std::size_t decision) {
      // Anything Gecode has no variable for is reported with the type spelled
      // out, so the message names what the model declared.
      std::string unsupported = "decision variable " + std::to_string(decision) +
                                " is declared `" + fznso::to_string(t) +
                                "`, which Gecode has no variables for";
      if (t.list_of)
        fail(unsupported);
      if (t.set_of) {
        if (t.base != FznsoTypeBaseInt)
          fail(unsupported);
        return VarKind::Set;
      }
      switch (t.base) {
      case FznsoTypeBaseBool:  return VarKind::Bool;
      case FznsoTypeBaseInt:   return VarKind::Int;
      case FznsoTypeBaseFloat: return VarKind::Float;
      default:
        fail(unsupported);
      }
    }

    void Translator::readTypes(std::size_t from, std::size_t to) {
      vars.resize(to);
      for (std::size_t d=from; d<to; d++) {
        fznso::Decision decision{d};
        vars[d].kind = kindOf(model.decision_type(decision), d);
        vars[d].index = -1;
      }
    }

    void Translator::addVars(FznsoSpace& space, std::size_t from, std::size_t to) {
      IntVarArgs iva;
      BoolVarArgs bva;
      std::vector<char> iIntro, iDep, bIntro, bDep;
#ifdef GECODE_HAS_SET_VARS
      SetVarArgs sva;
      std::vector<char> sIntro, sDep;
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      FloatVarArgs fva;
      std::vector<char> fIntro, fDep;
#endif

      for (std::size_t d=from; d<to; d++) {
        fznso::Decision decision{d};
        fznso::Value domain = model.decision_domain(decision);
        // A variable a solution may not be asked for is scaffolding, which is
        // what `createBranchers` uses to keep it out of the search. Names say
        // nothing about this — they exist only for debugging.
        char introduced = model.decision_in_solution(decision) ? 0 : 1;
        char funcDep = model.decision_defined(decision) ? 1 : 0;
        VarRef& ref = vars[d];
        switch (ref.kind) {
        case VarKind::Bool: {
          int min = 0;
          int max = 1;
          if (domain.kind() == FznsoValueSetInt) {
            IntSet d01 = toIntSet(domain);
            min = d01.size() == 0 ? 1 : std::max(0, d01.min());
            max = d01.size() == 0 ? 0 : std::min(1, d01.max());
          }
          if (min > max) {
            // The domain excludes both truth values; the variable itself is
            // still needed so that later indexes line up.
            min = 0;
            max = 1;
            space.fail();
          }
          ref.index = bva.size();
          bva << BoolVar(space, min, max);
          bIntro.push_back(introduced);
          bDep.push_back(funcDep);
          break;
        }
        case VarKind::Float: {
#ifdef GECODE_HAS_FLOAT_VARS
          FloatNum min = Float::Limits::min;
          FloatNum max = Float::Limits::max;
          if (domain.kind() == FznsoValueSetFloat && domain.size() > 0) {
            // Gecode float domains are single intervals, so several ranges
            // collapse into their hull.
            min = domain.float_range(0).min;
            max = domain.float_range(domain.size()-1).max;
          }
          ref.index = fva.size();
          fva << FloatVar(space, min, max);
          fIntro.push_back(introduced);
          fDep.push_back(funcDep);
#else
          fail("this Gecode build has no float variables");
#endif
          break;
        }
        case VarKind::Set: {
#ifdef GECODE_HAS_SET_VARS
          IntSet ub = domain.kind() == FznsoValueSetInt
            ? toIntSet(domain)
            : IntSet(Set::Limits::min, Set::Limits::max);
          ref.index = sva.size();
          sva << SetVar(space, IntSet::empty, ub);
          sIntro.push_back(introduced);
          sDep.push_back(funcDep);
#else
          fail("this Gecode build has no set variables");
#endif
          break;
        }
        default: {
          IntSet d = domain.kind() == FznsoValueSetInt
            ? toIntSet(domain)
            : IntSet(Int::Limits::min, Int::Limits::max);
          ref.index = iva.size();
          if (d.size() == 0) {
            // An empty domain is unsatisfiable; give the variable something
            // legal and let the space fail.
            iva << IntVar(space, 0, 0);
            space.fail();
          } else {
            iva << IntVar(space, d);
          }
          iIntro.push_back(introduced);
          iDep.push_back(funcDep);
          break;
        }
        }
      }

      // The indexes above are relative to this layer; shift them to the
      // position each variable takes in the space's arrays.
      int intBase = space.iv.size();
      int boolBase = space.bv.size();
#ifdef GECODE_HAS_SET_VARS
      int setBase = space.sv.size();
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      int floatBase = space.fv.size();
#endif
      for (std::size_t d=from; d<to; d++) {
        switch (vars[d].kind) {
        case VarKind::Bool:  vars[d].index += boolBase; break;
#ifdef GECODE_HAS_SET_VARS
        case VarKind::Set:   vars[d].index += setBase; break;
#endif
#ifdef GECODE_HAS_FLOAT_VARS
        case VarKind::Float: vars[d].index += floatBase; break;
#endif
        default:             vars[d].index += intBase; break;
        }
      }

      space.addIntVars(iva, iIntro, iDep);
      space.addBoolVars(bva, bIntro, bDep);
#ifdef GECODE_HAS_SET_VARS
      space.addSetVars(sva, sIntro, sDep);
#endif
#ifdef GECODE_HAS_FLOAT_VARS
      space.addFloatVars(fva, fIntro, fDep);
#endif
    }

    AST::Node* Translator::varNode(fznso::Decision d) const {
      if (d.index >= vars.size() || vars[d.index].index < 0)
        fail("constraint refers to decision variable " + std::to_string(d.index) +
             ", which is not in the model");
      const VarRef& ref = vars[d.index];
      std::optional<std::string_view> name = model.decision_name(d);
      std::string n = name.has_value() ? std::string(*name) : std::string();
      switch (ref.kind) {
      case VarKind::Bool:  return new AST::BoolVar(ref.index, n);
      case VarKind::Float: return new AST::FloatVar(ref.index, n);
      case VarKind::Set:   return new AST::SetVar(ref.index, n);
      default:             return new AST::IntVar(ref.index, n);
      }
    }

    AST::Node* Translator::node(const fznso::Value& value, const FznsoType& expected) const {
      switch (value.kind()) {
      case FznsoValueBool:
        return new AST::BoolLit(value.as_bool());
      case FznsoValueInt:
        // A float argument may legitimately be given an integer literal.
        if (expected.base == FznsoTypeBaseFloat && !expected.set_of)
          return new AST::FloatLit(static_cast<double>(value.as_int()));
        return new AST::IntLit(toInt(value.as_int()));
      case FznsoValueFloat:
        return new AST::FloatLit(value.as_float());
      case FznsoValueString:
        return new AST::Atom(std::string(value.as_string()));
      case FznsoValueDecision:
        return varNode(value.as_decision());
      case FznsoValueSetInt:
        return toSetLit(value);
      case FznsoValueList: {
        // The element type is the scalar form of the position's type; a nested
        // list (a flattened 2-D table, say) keeps the same base.
        FznsoType element = expected;
        element.list_of = false;
        AST::Array* a = new AST::Array(static_cast<int>(value.size()));
        for (std::size_t i=0; i<value.size(); i++)
          a->a[i] = node(value[i], element);
        return a;
      }
      case FznsoValueConstraint:
        fail("a constraint reference cannot be a constraint argument");
      case FznsoValueSetFloat:
        fail("Gecode has no float sets");
      default:
        fail("a constraint argument may not be absent");
      }
    }

    void Translator::postConstraints(FznsoSpace& space, std::size_t from, std::size_t to) {
      std::vector<ConExpr*> constraints;
      constraints.reserve(to-from);
      try {
        for (std::size_t c=from; c<to; c++) {
          fznso::Constraint con{c};
          std::string ident{model.constraint_ident(con)};
          if (ident.compare(0, 4, "fzn_") == 0)
            ident.erase(0, 4);
          // Recorded rather than posted; see `OnRestart`.
          if (collectOnRestart(ident, con))
            continue;
          const Signature* sig = signature(ident);
          if (sig == nullptr)
            fail("unknown constraint `" + ident + "`");
          std::size_t n = model.constraint_argument_count(con);
          AST::Array* args = new AST::Array(static_cast<int>(n));
          AST::Array* ann = new AST::Array(
            static_cast<int>(model.constraint_annotation_count(con)));
          // Own both before anything below can throw. The registry knows some
          // constraints under a name the model does not use, so the expression
          // is built with the one it will be looked up by.
          constraints.push_back(new ConExpr(std::string(sig->posts_as), args, ann));
          for (std::size_t a=0; a<n; a++) {
            // The poster's argument `a` is the declared argument `from`.
            std::size_t from = sig->permutation != nullptr && a < sig->permutation->size()
              ? (*sig->permutation)[a] : a;
            if (from >= n)
              fail("constraint `" + ident + "` was given " + std::to_string(n) +
                   " arguments, too few to reorder");
            args->a[a] = node(model.constraint_argument(con, from),
                              argument_type(*sig->declared, from));
          }
          for (std::size_t a=0; a<ann->a.size(); a++)
            ann->a[a] = annotationNode(model.constraint_annotation(con, a));
        }
      } catch (...) {
        for (ConExpr* ce : constraints)
          delete ce;
        throw;
      }
      // Takes ownership of the expressions, and deletes them as it posts.
      space.postConstraints(constraints);
    }

    AST::Node* Translator::annotationNode(const fznso::AnnotationRef& ann) const {
      std::string ident{ann.ident()};
      if (ann.size() == 0)
        return new AST::Atom(ident);
      AST::Array* args = new AST::Array(static_cast<int>(ann.size()));
      AST::Call* call = new AST::Call(ident, args);
      // An annotation carries no declared types, so each argument is read as
      // whatever it says it is; a string becomes an atom, which is how the
      // search annotations name their variable and value selections.
      static const FznsoType any = fznso::Type{FznsoTypeBaseInt}.decision(true);
      for (std::size_t i=0; i<ann.size(); i++)
        args->a[i] = node(ann[i], any);
      return call;
    }

    AST::Array* Translator::solveAnnotations() const {
      std::size_t n = model.objective_annotation_count();
      AST::Array* a = new AST::Array(static_cast<int>(n));
      try {
        for (std::size_t i=0; i<n; i++)
          a->a[i] = annotationNode(model.objective_annotation(i));
      } catch (...) {
        delete a;
        throw;
      }
      return a;
    }

  }

  // -------------------------------------------------------------------------
  // Reporting
  // -------------------------------------------------------------------------

  namespace {

    /// One solution, with every decision's value copied out of the space.
    ///
    /// The protocol hands out borrowed values, so the payloads have to outlive
    /// the `on_solution` callback; owning them here is the simplest way to
    /// guarantee it.
    class Solution : public fznso::SolutionSource {
    public:
      Solution(const FlatZincSpace& space, const std::vector<VarRef>& vars,
               const Search::Statistics& stat, std::int64_t solutions) {
        values.reserve(vars.size());
        for (const VarRef& ref : vars)
          values.push_back(read(space, ref));
        add("solutions", solutions);
        add("nodes", static_cast<std::int64_t>(stat.node));
        add("failures", static_cast<std::int64_t>(stat.fail));
        add("propagations", static_cast<std::int64_t>(stat.propagate));
        add("restarts", static_cast<std::int64_t>(stat.restart));
        add("peak_depth", static_cast<std::int64_t>(stat.depth));
      }

      fznso::Value value(fznso::Decision decision) const override {
        return decision.index < values.size() ? fznso::Value{values[decision.index]}
                                              : fznso::Value{};
      }

      fznso::Value statistic(std::string_view name) const override {
        for (const std::pair<std::string, fznso::OwnedValue>& s : stats)
          if (s.first == name)
            return fznso::Value{s.second};
        return fznso::Value{};
      }

    private:
      void add(const char* name, std::int64_t v) { stats.emplace_back(name, v); }

      /// A variable's value, or absent if the search left it unassigned.
      ///
      /// That should not happen — every variable is either branched on or
      /// functionally defined — but a model may claim `defined` for a variable
      /// no constraint actually determines, and reading `val()` off an
      /// unassigned variable is undefined behaviour rather than an error.
      static fznso::OwnedValue read(const FlatZincSpace& space, const VarRef& ref) {
        if (ref.index < 0)
          return fznso::OwnedValue{};
        switch (ref.kind) {
        case VarKind::Bool:
          if (!space.bv[ref.index].assigned())
            return fznso::OwnedValue{};
          return fznso::OwnedValue{space.bv[ref.index].val() != 0};
#ifdef GECODE_HAS_FLOAT_VARS
        case VarKind::Float:
          // A float variable is only ever narrowed to an interval, so the
          // reported value is its midpoint, as the FlatZinc interpreter prints.
          return fznso::OwnedValue{space.fv[ref.index].med()};
#endif
#ifdef GECODE_HAS_SET_VARS
        case VarKind::Set: {
          if (!space.sv[ref.index].assigned())
            return fznso::OwnedValue{};
          std::vector<fznso::Range<std::int64_t> > ranges;
          for (SetVarGlbRanges r(space.sv[ref.index]); r(); ++r)
            ranges.push_back({r.min(), r.max()});
          return fznso::OwnedValue{std::move(ranges)};
        }
#endif
        default:
          if (!space.iv[ref.index].assigned())
            return fznso::OwnedValue{};
          return fznso::OwnedValue{static_cast<std::int64_t>(space.iv[ref.index].val())};
        }
      }

      std::vector<fznso::OwnedValue> values;
      std::vector<std::pair<std::string, fznso::OwnedValue> > stats;
    };

    /// Stops the search when the caller asks, on top of whatever limits the
    /// options set.
    class StopSignalStop : public Search::Stop {
    public:
      StopSignalStop(Search::Stop* inner, const fznso::StopSignal& signal)
        : inner(inner), signal(signal) {}
      bool stop(const Search::Statistics& s, const Search::Options& o) override {
        if (signal.requested())
          return true;
        return inner.get() != nullptr && inner->stop(s, o);
      }
    private:
      std::unique_ptr<Search::Stop> inner;
      const fznso::StopSignal& signal;
    };

  }

  // -------------------------------------------------------------------------
  // Solver
  // -------------------------------------------------------------------------

  /// Everything that survives between runs.
  class GecodeSolver::State {
  public:
    State() : random(1U), root(nullptr) {}
    ~State() { reset(); }

    /// Drop every space, including the root.
    void reset() {
      for (FznsoSpace* s : layers)
        delete s;
      layers.clear();
      delete root;
      root = nullptr;
      vars.clear();
      onRestart.clear();
      decisionEnd.clear();
      constraintEnd.clear();
    }

    Settings settings;
    Rnd random;
    /// The empty space every layer is ultimately cloned from.
    FznsoSpace* root;
    /// One space per model layer, holding layers `0..=i`. A null entry means
    /// the model is already unsatisfiable at that layer.
    std::vector<FznsoSpace*> layers;
    /// How many decisions and constraints each layer accounts for.
    std::vector<std::size_t> decisionEnd;
    std::vector<std::size_t> constraintEnd;
    /// Where each decision variable ended up, indexed by decision.
    std::vector<VarRef> vars;
    /// The `on_restart` constraints across every layer, materialised onto the
    /// space each run searches.
    OnRestart onRestart;

    // Statistics from the last run.
    Search::Statistics stat;
    std::int64_t solutions = 0;
    std::int64_t propagators = 0;
    double initTime = 0.0;
    double solveTime = 0.0;
    /// A statistic is handed out as a borrowed value, so each one needs its own
    /// storage that outlives the call. A node-based container keeps every entry
    /// put as later ones are added.
    mutable std::map<std::string, fznso::OwnedValue> statCache;
  };

  GecodeSolver::GecodeSolver() : state(new State) {}
  GecodeSolver::~GecodeSolver() { delete state; }

  // --- options -------------------------------------------------------------

  namespace {

    /// An option value the solver hands back. It borrows, so the payload has to
    /// live somewhere stable: each option reads back from `Settings`.
    fznso::Value optional(bool present, const std::int64_t& value) {
      return present ? fznso::Value{value} : fznso::Value{};
    }

  }

  fznso::Value GecodeSolver::option_get(std::string_view name) const {
    const Settings& o = state->settings;
    if (name == "all_solutions") return fznso::Value{o.all_solutions};
    if (name == "fixed_search") return fznso::Value{o.fixed_search};
    if (name == "intermediate") return fznso::Value{o.intermediate};
    if (name == "verbose") return fznso::Value{o.verbose};
    if (name == "threads") return fznso::Value{o.threads};
    if (name == "solution_limit") return fznso::Value{o.solution_limit};
    if (name == "time_limit") return optional(o.has_time_limit, o.time_limit);
    if (name == "random_seed") return optional(o.has_seed, o.seed);
    if (name == "node_limit") return optional(o.has_node_limit, o.node_limit);
    if (name == "fail_limit") return optional(o.has_fail_limit, o.fail_limit);
    if (name == "restart_limit") return optional(o.has_restart_limit, o.restart_limit);
    if (name == "restart") return fznso::Value{o.restart};
    if (name == "restart_base") return fznso::Value{o.restart_base};
    if (name == "restart_scale") return fznso::Value{o.restart_scale};
    if (name == "nogoods") return fznso::Value{o.nogoods};
    if (name == "nogoods_limit") return fznso::Value{o.nogoods_limit};
    if (name == "c_d") return fznso::Value{o.c_d};
    if (name == "a_d") return fznso::Value{o.a_d};
    if (name == "decay") return fznso::Value{o.decay};
    if (name == "float_step") return fznso::Value{o.step};
    return fznso::Value{};
  }

  std::optional<std::string>
  GecodeSolver::option_set(std::string_view name, fznso::Value value) {
    Settings& o = state->settings;
    std::string ident{name};

    auto wrongType = [&ident](const char* expected) {
      return std::optional<std::string>{"option `" + ident + "` expects " + expected};
    };
    auto asBool = [&](bool& target) -> std::optional<std::string> {
      if (value.kind() != FznsoValueBool)
        return wrongType("a Boolean");
      target = value.as_bool();
      return std::nullopt;
    };
    auto asInt = [&](std::int64_t& target) -> std::optional<std::string> {
      if (value.kind() != FznsoValueInt)
        return wrongType("an integer");
      target = value.as_int();
      return std::nullopt;
    };
    // An optional integer: absent clears the limit.
    auto asOptInt = [&](bool& present, std::int64_t& target) -> std::optional<std::string> {
      if (value.kind() == FznsoValueAbsent) {
        present = false;
        return std::nullopt;
      }
      if (value.kind() != FznsoValueInt)
        return wrongType("an integer or nothing");
      if (value.as_int() < 1)
        return std::optional<std::string>{"option `" + ident + "` must be positive"};
      present = true;
      target = value.as_int();
      return std::nullopt;
    };
    auto asFloat = [&](double& target) -> std::optional<std::string> {
      if (value.kind() == FznsoValueInt) {
        target = static_cast<double>(value.as_int());
        return std::nullopt;
      }
      if (value.kind() != FznsoValueFloat)
        return wrongType("a float");
      target = value.as_float();
      return std::nullopt;
    };

    if (name == "all_solutions") return asBool(o.all_solutions);
    if (name == "fixed_search") return asBool(o.fixed_search);
    if (name == "intermediate") return asBool(o.intermediate);
    if (name == "verbose") return asBool(o.verbose);
    if (name == "nogoods") return asBool(o.nogoods);
    if (name == "threads") {
      std::optional<std::string> e = asInt(o.threads);
      if (!e.has_value() && o.threads == 0)
        return std::optional<std::string>{"option `threads` must not be zero"};
      return e;
    }
    if (name == "solution_limit") {
      std::optional<std::string> e = asInt(o.solution_limit);
      if (!e.has_value() && o.solution_limit < 0)
        return std::optional<std::string>{"option `solution_limit` must not be negative"};
      return e;
    }
    if (name == "nogoods_limit") return asInt(o.nogoods_limit);
    if (name == "c_d") return asInt(o.c_d);
    if (name == "a_d") return asInt(o.a_d);
    if (name == "restart_scale") return asInt(o.restart_scale);
    if (name == "time_limit") return asOptInt(o.has_time_limit, o.time_limit);
    if (name == "random_seed") return asOptInt(o.has_seed, o.seed);
    if (name == "node_limit") return asOptInt(o.has_node_limit, o.node_limit);
    if (name == "fail_limit") return asOptInt(o.has_fail_limit, o.fail_limit);
    if (name == "restart_limit") return asOptInt(o.has_restart_limit, o.restart_limit);
    if (name == "restart_base") return asFloat(o.restart_base);
    if (name == "decay") return asFloat(o.decay);
    if (name == "float_step") return asFloat(o.step);
    if (name == "restart") {
      if (value.kind() != FznsoValueString)
        return wrongType("a string");
      std::string mode{value.as_string()};
      bool known = false;
      (void) restartMode(mode, known);
      if (!known)
        return std::optional<std::string>{
          "unknown restart sequence `" + mode +
          "` (expected none, constant, linear, luby or geometric)"};
      o.restart = mode;
      return std::nullopt;
    }
    return std::optional<std::string>{"unknown option `" + ident + "`"};
  }

  fznso::Value GecodeSolver::statistic(std::string_view name) const {
    const State& s = *state;
    fznso::OwnedValue value;
    if (name == "solutions") value = fznso::OwnedValue{s.solutions};
    else if (name == "nodes") value = fznso::OwnedValue{static_cast<std::int64_t>(s.stat.node)};
    else if (name == "failures") value = fznso::OwnedValue{static_cast<std::int64_t>(s.stat.fail)};
    else if (name == "propagations") value = fznso::OwnedValue{static_cast<std::int64_t>(s.stat.propagate)};
    else if (name == "restarts") value = fznso::OwnedValue{static_cast<std::int64_t>(s.stat.restart)};
    else if (name == "peak_depth") value = fznso::OwnedValue{static_cast<std::int64_t>(s.stat.depth)};
    else if (name == "propagators") value = fznso::OwnedValue{s.propagators};
    else if (name == "variables") value = fznso::OwnedValue{static_cast<std::int64_t>(s.vars.size())};
    else if (name == "init_time") value = fznso::OwnedValue{s.initTime};
    else if (name == "solve_time") value = fznso::OwnedValue{s.solveTime};
    else return fznso::Value{};

    fznso::OwnedValue& slot = s.statCache[std::string(name)];
    slot = std::move(value);
    return fznso::Value{slot};
  }

  // --- run -----------------------------------------------------------------

  namespace {

    /// Everything one call to `run` needs, threaded through the search
    /// templates below.
    struct Run {
      const fznso::Model& model;
      fznso::SolutionSink& solutions;
      fznso::MessageSink& messages;
      const fznso::StopSignal& stop;
      GecodeSolver::State& state;
      // `FlatZincOptions` holds a list of its own members, so it must be
      // referred to rather than copied.
      Options& opt;
      /// How many solutions to report before giving up, or 0 for no limit.
      std::int64_t limit = 0;
      /// Whether to report solutions as they improve, or only the last one.
      bool reportEach = true;
      bool stopped = false;
      bool limitReached = false;

      void log(const std::string& text) {
        if (state.settings.verbose && messages.wanted())
          messages.message("log", fznso::Value{text});
      }
      void warn(const std::string& text) {
        if (messages.wanted())
          messages.message("warn", fznso::Value{text});
      }
      void bound(double value) {
        if (messages.wanted())
          messages.message("progress.bound", fznso::Value{value});
      }
    };

    /// Drive one search engine, reporting through the sinks.
    ///
    /// This is `FlatZincSpace::runMeta` with the printing replaced by
    /// callbacks; the engine set-up is deliberately the same, so a run through
    /// this interface explores the same tree as `fzn-gecode` would.
    template<template<class> class Engine,
             template<class, template<class> class> class Meta>
    std::unique_ptr<FlatZincSpace> runMeta(Run& run, FznsoSpace* space) {
      Search::Options o;
      std::unique_ptr<Search::Stop> limits(
        Driver::CombinedStop::create(run.opt.node(), run.opt.fail(), run.opt.time(),
                                     run.opt.restart_limit(), true));
      StopSignalStop stop(limits.release(), run.stop);
      o.stop = &stop;
      o.c_d = run.opt.c_d();
      o.a_d = run.opt.a_d();
      o.threads = run.opt.threads();
      o.nogoods_limit = run.opt.nogoods() ? run.opt.nogoods_limit() : 0;
      o.cutoff = new Search::CutoffAppend(new Search::CutoffConstant(0), 1,
                                          Driver::createCutoff(run.opt));

      std::unique_ptr<FlatZincSpace> best;
      bool optimising = space->method() != FlatZincSpace::SAT;
      Meta<FlatZincSpace, Engine> se(space, o);
      while (FlatZincSpace* next = se.next()) {
        std::unique_ptr<FlatZincSpace> solution(next);
        run.state.solutions++;
        run.state.stat = se.statistics();
        if (run.reportEach) {
          Solution reported(*solution, run.state.vars, run.state.stat,
                            run.state.solutions);
          run.solutions.solution(reported);
          if (optimising && run.messages.wanted()) {
            int optVar = solution->optVar();
            if (solution->optVarIsInt())
              run.bound(static_cast<double>(solution->iv[optVar].val()));
#ifdef GECODE_HAS_FLOAT_VARS
            else
              run.bound(solution->fv[optVar].med());
#endif
          }
          // Polled here so that a caller can answer "that one will do" from
          // inside its own callback.
          if (run.stop.requested()) {
            run.stopped = true;
            best = std::move(solution);
            break;
          }
        }
        best = std::move(solution);
        if (run.limit != 0 && run.state.solutions >= run.limit) {
          run.limitReached = true;
          break;
        }
      }
      if (!run.stopped)
        run.stopped = se.stopped();
      run.state.stat = se.statistics();
      return best;
    }

    template<template<class> class Engine>
    std::unique_ptr<FlatZincSpace> runEngine(Run& run, FznsoSpace* space) {
      if (run.opt.restart() == RM_NONE)
        return runMeta<Engine, Driver::EngineToMeta>(run, space);
      return runMeta<Engine, RBS>(run, space);
    }

  }

  namespace {

  /// One run, start to finish. Model errors surface as exceptions, which
  /// `GecodeSolver::run` turns into the protocol's error status.
  fznso::Status
  runOnce(GecodeSolver::State& s, const fznso::Model& model,
          fznso::SolutionSink& solutions, fznso::MessageSink& messages,
          const fznso::StopSignal& stop) {
    Support::Timer total;
    total.start();

    // A caller that has already given up gets no search at all.
    if (stop.requested())
      return fznso::Status{fznso::Status::Kind::Incomplete, {}};

    Options options;
    Run run{model, solutions, messages, stop, s, options};
    const Settings& set = s.settings;

    // --- options into Gecode's own option object -------------------------
    run.opt.seed(set.has_seed ? static_cast<int>(set.seed) : 0);
    run.opt.decay(set.decay);
    run.opt.threads(static_cast<double>(set.threads));
    run.opt.time(set.has_time_limit ? static_cast<double>(set.time_limit) : 0.0);
    run.opt.node(set.has_node_limit ? static_cast<unsigned long long int>(set.node_limit) : 0);
    run.opt.fail(set.has_fail_limit ? static_cast<unsigned long long int>(set.fail_limit) : 0);
    run.opt.restart_limit(set.has_restart_limit
                          ? static_cast<unsigned long long int>(set.restart_limit) : 0);
    run.opt.c_d(static_cast<unsigned int>(set.c_d));
    run.opt.a_d(static_cast<unsigned int>(set.a_d));
    run.opt.nogoods(set.nogoods);
    run.opt.nogoods_limit(static_cast<unsigned int>(set.nogoods_limit));
    run.opt.step(set.step);
    bool knownRestart = false;
    run.opt.restart(restartMode(set.restart, knownRestart));
    run.opt.restart_base(set.restart_base);
    run.opt.restart_scale(static_cast<int>(set.restart_scale));
    s.random = Rnd(static_cast<unsigned int>(set.has_seed ? set.seed : 1));

    s.solutions = 0;
    s.stat = Search::Statistics();

    // --- reconcile the layer clones with the model ------------------------
    std::size_t layerCount = model.layer_count();
    std::size_t permanent = std::min(model.layer_permanent(), layerCount);
    // Layers at or above `unchanged` may have been replaced, so their clones
    // are dropped and rebuilt. Permanent layers can never be retracted, so a
    // consumer that under-reports `unchanged` cannot invalidate them.
    std::size_t keep = std::min(std::max(model.layer_unchanged(), permanent),
                                std::min(s.layers.size(), layerCount));
    for (std::size_t i=s.layers.size(); i>keep; i--)
      delete s.layers[i-1];
    s.layers.resize(keep);
    s.decisionEnd.resize(keep);
    s.constraintEnd.resize(keep);
    s.vars.resize(keep == 0 ? 0 : s.decisionEnd.back());

    Translator translator(model, s.vars, s.onRestart);

    if (s.root == nullptr) {
      s.root = new FznsoSpace(s.random);
      s.root->init(0, 0, 0, 0);
    }

    for (std::size_t layer=keep; layer<layerCount; layer++) {
      std::size_t dFrom = layer == 0 ? 0 : model.decision_layer_end(layer-1);
      std::size_t dTo = model.decision_layer_end(layer);
      std::size_t cFrom = layer == 0 ? 0 : model.constraint_layer_end(layer-1);
      std::size_t cTo = model.constraint_layer_end(layer);
      translator.readTypes(dFrom, dTo);

      FznsoSpace* base = layer == 0 ? s.root : s.layers[layer-1];
      FznsoSpace* built = nullptr;
      if (base != nullptr) {
        // A space has to be stable before it can be cloned.
        if (base->status() == SS_FAILED) {
          run.log("layer " + std::to_string(layer-1) + " is already unsatisfiable");
        } else {
          built = static_cast<FznsoSpace*>(base->clone());
          built->reopen(*base);
          translator.addVars(*built, dFrom, dTo);
          translator.postConstraints(*built, cFrom, cTo);
        }
      }
      s.layers.push_back(built);
      s.decisionEnd.push_back(dTo);
      s.constraintEnd.push_back(cTo);
      run.log("posted layer " + std::to_string(layer) + ": " +
              std::to_string(dTo-dFrom) + " decision(s), " +
              std::to_string(cTo-cFrom) + " constraint(s)");
    }

    FznsoSpace* top = s.layers.empty() ? s.root : s.layers.back();
    s.initTime = total.stop();
    if (top == nullptr || top->status() == SS_FAILED) {
      s.solveTime = 0.0;
      return fznso::Status{fznso::Status::Kind::Complete, {}};
    }

    // --- the run space: objective, branchers, search ----------------------
    std::string_view objective = model.objective_ident();
    bool optimising = !objective.empty();
    bool maximise = objective.compare(0, 8, "maximize") == 0;
    if (optimising && !maximise && objective.compare(0, 8, "minimize") != 0)
      return fznso::Status{fznso::Status::Kind::Error,
                           "unsupported objective strategy `" + std::string(objective) + "`"};

    VarRef optRef;
    if (optimising) {
      fznso::Value arg = model.objective_arg();
      if (arg.kind() != FznsoValueDecision)
        return fznso::Status{fznso::Status::Kind::Error,
                             "an objective takes a single decision variable"};
      std::size_t index = arg.as_decision().index;
      if (index >= s.vars.size())
        return fznso::Status{fznso::Status::Kind::Error, "the objective is not in the model"};
      optRef = s.vars[index];
      if (optRef.kind != VarKind::Int && optRef.kind != VarKind::Float)
        return fznso::Status{fznso::Status::Kind::Error,
                             "only integer and float objectives are supported"};
    }

    // Free search means Gecode picks its own branchers, which is what it does
    // for a FlatZinc model that carries no search annotation.
    std::unique_ptr<AST::Array> searchAnn(
      set.fixed_search ? translator.solveAnnotations() : nullptr);

    Support::Timer solve;
    solve.start();

    // A fresh space to search in: the objective and the branchers belong to one
    // run, never to a layer, so they go on a throwaway clone.
    auto buildRunSpace = [&](bool asSatisfaction) {
      std::unique_ptr<FznsoSpace> rs(static_cast<FznsoSpace*>(top->clone()));
      rs->reopen(*top);
      // The `on_restart` arrays belong to one run, like the branchers: they are
      // sized from every layer's constraints, so they go on the clone.
      applyOnRestart(*rs, s.onRestart);
      if (!optimising || asSatisfaction) {
        rs->solve(nullptr);
      } else if (maximise) {
        rs->maximize(optRef.index, optRef.kind == VarKind::Int, nullptr);
      } else {
        rs->minimize(optRef.index, optRef.kind == VarKind::Int, nullptr);
      }
      return rs;
    };

    // `createBranchers` names every variable it branches on, so the printer has
    // to hold a name for each. The model's own names are not usable: they are
    // optional, and indexed by decision rather than by Gecode variable.
    auto branch = [&](FznsoSpace& space, AST::Node* ann) {
      Printer printer;
      for (int i=0; i<space.iv.size(); i++) printer.addIntVarName("x" + std::to_string(i));
      for (int i=0; i<space.bv.size(); i++) printer.addBoolVarName("b" + std::to_string(i));
#ifdef GECODE_HAS_FLOAT_VARS
      for (int i=0; i<space.fv.size(); i++) printer.addFloatVarName("f" + std::to_string(i));
#endif
#ifdef GECODE_HAS_SET_VARS
      for (int i=0; i<space.sv.size(); i++) printer.addSetVarName("s" + std::to_string(i));
#endif
      std::ostringstream warnings;
      space.createBranchers(printer, ann, run.opt, true, warnings);
      if (!warnings.str().empty())
        run.warn(warnings.str());
    };

    fznso::Status status{fznso::Status::Kind::Complete, {}};
    StatusStatistics sstat;
    if (top->status(sstat) != SS_FAILED)
      s.propagators = PropagatorGroup::all.size(*top);

    // Every solution attaining the optimum is a second search, so the first
    // one need not report anything but the improving solutions it is asked for.
    bool enumerateOptima = optimising && set.all_solutions;

    std::unique_ptr<FznsoSpace> runSpace = buildRunSpace(false);
    branch(*runSpace, searchAnn.get());

    // Reporting policy. A satisfaction problem reports every solution it is
    // asked for; an optimisation problem reports improving ones, and only the
    // last of them unless `intermediate` says otherwise.
    run.reportEach = !optimising || set.intermediate;
    if (optimising || set.solution_limit != 0) {
      run.limit = set.solution_limit;
    } else {
      run.limit = set.all_solutions ? 0 : 1;
    }

    std::unique_ptr<FlatZincSpace> best =
      optimising ? runEngine<BAB>(run, runSpace.get())
                 : runEngine<DFS>(run, runSpace.get());

    if (best && !run.reportEach && !enumerateOptima) {
      Solution reported(*best, s.vars, s.stat, s.solutions);
      solutions.solution(reported);
      if (stop.requested())
        run.stopped = true;
    }

    // Branch-and-bound only ever looks for something strictly better, so it
    // never enumerates the ties. Pinning the objective to the value it found
    // and searching again does.
    if (enumerateOptima && best && !run.stopped) {
      std::unique_ptr<FznsoSpace> second = buildRunSpace(true);
      if (optRef.kind == VarKind::Int) {
        rel(*second, second->iv[optRef.index], IRT_EQ, best->iv[optRef.index].val());
      }
#ifdef GECODE_HAS_FLOAT_VARS
      else {
        rel(*second, second->fv[optRef.index], FRT_EQ, best->fv[optRef.index].med());
      }
#endif
      branch(*second, nullptr);
      run.reportEach = true;
      run.limit = 0;
      best = runEngine<DFS>(run, second.get());
    }

    if (run.stopped || run.limitReached)
      status.kind = fznso::Status::Kind::Incomplete;

    s.solveTime = solve.stop();
    return status;
  }

  }

  fznso::Status
  GecodeSolver::run(const fznso::Model& model, fznso::SolutionSink& solutions,
                    fznso::MessageSink& messages, const fznso::StopSignal& stop) {
    try {
      return runOnce(*state, model, solutions, messages, stop);
    } catch (FlatZinc::Error& e) {
      // A run that threw may have left a layer half-built, so the incremental
      // state is thrown away rather than reused against a model it no longer
      // matches.
      state->reset();
      return fznso::Status{fznso::Status::Kind::Error, e.toString()};
    } catch (Gecode::Exception& e) {
      state->reset();
      return fznso::Status{fznso::Status::Kind::Error, e.what()};
    } catch (AST::TypeError& e) {
      state->reset();
      return fznso::Status{fznso::Status::Kind::Error, e.what()};
    }
  }

}}

FZNSO_EXPORT_SOLVER(Gecode::FlatZinc::GecodeSolver, gecode);
