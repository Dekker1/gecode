/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Gecode behind the FZnSO solver protocol.
 *
 *  FZnSO drives a solver through thirteen C entry points exported from a shared
 *  object, handing it a model of decision variables and constraints instead of
 *  a FlatZinc file. The shapes line up closely with what the FlatZinc
 *  interpreter already builds, so this is a translation layer over
 *  `FlatZincSpace` and `registry()` rather than a second interpreter: the same
 *  variable creation, the same constraint posting, the same branchers.
 *
 *  What is genuinely different is the lifecycle. A FlatZinc run parses a file
 *  once and searches once; an FZnSO consumer builds a model in *layers* and runs
 *  repeatedly, adding and retracting layers in between. Gecode cannot retract a
 *  posted constraint, so a layer is represented by a clone of the space below
 *  it: retracting one is discarding its clone.
 */

#ifndef GECODE_FLATZINC_FZNSO_HH
#define GECODE_FLATZINC_FZNSO_HH

#include <gecode/flatzinc.hh>

#include <gecode/third-party/fznso/fznso_export.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Gecode { namespace FlatZinc {

  /// Which of Gecode's variable arrays a decision variable lives in.
  ///
  /// Taken straight from the model's `decision_type`: a domain could not stand
  /// in for it, since an absent domain is equally a `var bool` and an unbounded
  /// `var int`, and an integer range list is equally a `var int` and the upper
  /// bound of a `var set of int`.
  enum class VarKind { Int, Bool, Float, Set };

  /// Where a decision variable ended up in the space.
  struct VarRef {
    VarKind kind = VarKind::Int;
    /// Index into `iv` / `bv` / `fv` / `sv`, by `kind`.
    int index = -1;
  };

  /// A declared constraint, together with the identifier Gecode's registry
  /// knows it by.
  ///
  /// The two differ for the constraints Gecode's MiniZinc library rewrites: it
  /// registers those under a `gecode_` prefix, but the prefix says where the
  /// constraint came from rather than what it is, so it is declared — and
  /// accepted — without one.
  struct Signature {
    /// The identifier and argument types reported to a consumer.
    const FznsoConstraintType* declared;
    /// The identifier to post it under, which `Registry::post` will find.
    std::string_view posts_as;
    /// Which declared argument each posted argument is taken from, for the
    /// constraints MiniZinc spells with the arguments in another order than
    /// the poster reads them. Null when the two orders agree.
    const std::vector<std::size_t>* permutation;
  };

  /// The declared signature of `ident`, or null if this solver does not accept
  /// it. A `fzn_` or `gecode_` prefix is stripped if the full name is unknown,
  /// so a model may spell a constraint either way.
  const Signature* signature(std::string_view ident);

  /// The argument type at `index`, saturating at the last declared one so that
  /// a trailing optional argument (Gecode accepts a reification on
  /// `array_bool_and` and friends) is typed like the argument before it.
  const FznsoType& argument_type(const FznsoConstraintType& sig, std::size_t index);

  /**
   * \brief A %FlatZincSpace that can still be extended after being cloned
   *
   * A layer clone has to accept new variables and constraints, which a plain
   * clone cannot: `FlatZincSpace::init` sizes the variable arrays once, and the
   * copy constructor drops the posting state (restored by
   * `FlatZincSpace::reopen`).
   */
  class FznsoSpace : public FlatZincSpace {
  public:
    explicit FznsoSpace(Rnd& random) : FlatZincSpace(random) {}
    FznsoSpace(FznsoSpace& f) : FlatZincSpace(f) {}

    Gecode::Space* copy(void) override { return new FznsoSpace(*this); }

    /// Append integer variables, recording whether each is introduced and
    /// functionally defined.
    void addIntVars(const IntVarArgs& add, const std::vector<char>& introduced,
                    const std::vector<char>& funcDep);
    /// Append Boolean variables.
    void addBoolVars(const BoolVarArgs& add, const std::vector<char>& introduced,
                     const std::vector<char>& funcDep);
#ifdef GECODE_HAS_SET_VARS
    /// Append set variables.
    void addSetVars(const SetVarArgs& add, const std::vector<char>& introduced,
                    const std::vector<char>& funcDep);
#endif
#ifdef GECODE_HAS_FLOAT_VARS
    /// Append float variables.
    void addFloatVars(const FloatVarArgs& add, const std::vector<char>& introduced,
                      const std::vector<char>& funcDep);
#endif
  };

  /**
   * \brief Gecode as an FZnSO solver
   *
   * Exported by `FZNSO_EXPORT_SOLVER` at the bottom of `fznso.cpp`.
   */
  class GecodeSolver : public fznso::Solver {
  public:
    GecodeSolver();
    ~GecodeSolver() override;

    fznso::Value option_get(std::string_view name) const override;
    std::optional<std::string> option_set(std::string_view name,
                                          fznso::Value value) override;
    fznso::Value statistic(std::string_view name) const override;
    fznso::Status run(const fznso::Model& model, fznso::SolutionSink& solutions,
                      fznso::MessageSink& messages,
                      const fznso::StopSignal& stop) override;

    /// The constraints this solver accepts, with their argument types. Also
    /// what tells an integer literal in a float argument position from one in
    /// an integer position when the argument AST is built.
    static FznsoConstraintList constraint_list();
    /// The decision-variable types this solver accepts.
    static FznsoTypeList decision_list();
    /// The objective strategies this solver supports.
    static FznsoObjectiveList objective_list();
    /// The options this solver accepts.
    static FznsoOptionList option_list();
    /// The statistics this solver reports.
    static FznsoStatisticList statistic_list();

    /// Everything that survives between runs: the layer clones, the decision
    /// type map and the option values. Defined in `fznso.cpp`.
    class State;

  private:
    State* state;
  };

}}

#endif

// STATISTICS: flatzinc-any
