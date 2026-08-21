/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  What Gecode declares to an FZnSO consumer: the constraints it accepts, the
 *  decision-variable types, the objective strategies, the options and the
 *  statistics.
 *
 *  The constraint table is what `fznso_gecode_constraint_list` reports, and it
 *  is also what tells an integer literal standing in a float argument position
 *  from one standing in an integer position when the argument AST is built.
 *
 *  Gecode registers the constraints its MiniZinc library rewrites under a
 *  `gecode_` prefix. The prefix records where a constraint came from rather
 *  than what it is, so `addGecode` declares those without it and remembers the
 *  prefixed name to post them under.
 *
 *  The signatures were read off `registry.cpp` — which argument each poster
 *  passes to `arg2intvarargs`, `arg2BoolVar`, `getInt` and friends — and
 *  cross-checked against the predicate declarations in the `mznlib` library and
 *  the preambles of the `test/flatzinc` fixtures. The `#ifdef`s mirror the ones
 *  guarding the matching `registry().add` calls, so the declared list is
 *  exactly what this build can post.
 */

#include <gecode/flatzinc/fznso.hh>

#include <cstdint>
#include <deque>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace Gecode { namespace FlatZinc {

  namespace {

    using fznso::str;

    // Argument type shorthands, built with the same builder the other language
    // bindings use. `V` marks the decision-variable form, `A` the array form.
    constexpr FznsoType B = fznso::Type{FznsoTypeBaseBool};
    constexpr FznsoType VB = fznso::Type{FznsoTypeBaseBool}.decision(true);
    constexpr FznsoType I = fznso::Type{FznsoTypeBaseInt};
    constexpr FznsoType VI = fznso::Type{FznsoTypeBaseInt}.decision(true);
    constexpr FznsoType F = fznso::Type{FznsoTypeBaseFloat};
    constexpr FznsoType VF = fznso::Type{FznsoTypeBaseFloat}.decision(true);
    constexpr FznsoType S = fznso::Type{FznsoTypeBaseInt}.set(true);
    constexpr FznsoType VS = fznso::Type{FznsoTypeBaseInt}.decision(true).set(true);

    constexpr FznsoType AB = fznso::Type{FznsoTypeBaseBool}.list(true);
    constexpr FznsoType AVB = fznso::Type{FznsoTypeBaseBool}.list(true).decision(true);
    constexpr FznsoType AI = fznso::Type{FznsoTypeBaseInt}.list(true);
    constexpr FznsoType AVI = fznso::Type{FznsoTypeBaseInt}.list(true).decision(true);
    constexpr FznsoType AF = fznso::Type{FznsoTypeBaseFloat}.list(true);
    constexpr FznsoType AVF = fznso::Type{FznsoTypeBaseFloat}.list(true).decision(true);
    constexpr FznsoType AS = fznso::Type{FznsoTypeBaseInt}.list(true).set(true);
    constexpr FznsoType AVS =
      fznso::Type{FznsoTypeBaseInt}.list(true).decision(true).set(true);

    /// The declared constraints, holding the identifiers and argument arrays
    /// the `FznsoConstraintType`s point at.
    ///
    /// Both are `std::deque`s because they hand out pointers into themselves,
    /// and a deque never moves what it already holds.
    class Table {
    public:
      /// Declare a constraint that Gecode registers under its own name.
      void add(const std::string& ident, std::initializer_list<FznsoType> args) {
        addVector(ident, ident, std::vector<FznsoType>(args));
      }

      /// Declare a constraint that Gecode registers as `gecode_<ident>`.
      void addGecode(const std::string& ident, std::initializer_list<FznsoType> args) {
        addVector(ident, "gecode_" + ident, std::vector<FznsoType>(args));
      }

      /// Declare the plain constraint together with its `_reif` and `_imp`
      /// forms, which take one extra `var bool`.
      void addReified(const std::string& ident, std::initializer_list<FznsoType> args) {
        std::vector<FznsoType> reified(args);
        reified.push_back(VB);
        add(ident, args);
        addVector(ident + "_reif", ident + "_reif", reified);
        addVector(ident + "_imp", ident + "_imp", reified);
      }

      /// As `addReified`, for a constraint registered under a `gecode_` prefix.
      void addGecodeReified(const std::string& ident, std::initializer_list<FznsoType> args) {
        std::vector<FznsoType> reified(args);
        reified.push_back(VB);
        addGecode(ident, args);
        addVector(ident + "_reif", "gecode_" + ident + "_reif", reified);
        addVector(ident + "_imp", "gecode_" + ident + "_imp", reified);
      }

      /// Declare `ident`, to be posted as `posts_as`.
      void addVector(std::string ident, std::string posts_as,
                     const std::vector<FznsoType>& args) {
        names.push_back(std::move(ident));
        posting.push_back(std::move(posts_as));
        arguments.push_back(args);
        const std::vector<FznsoType>& a = arguments.back();
        types.push_back(FznsoConstraintType{str(names.back()), a.size(), a.data()});
        // A name may be declared more than once, for the argument types a
        // solver accepts it with — `set_in` over a fixed and over a variable
        // set, say. Only the first is indexed: the entry is what a model
        // identifier resolves to, and the forms of one constraint post the same
        // way and read their arguments the same way.
        index.emplace(names.back(), signatures.size());
        // `types` is only filled in below, once it has stopped reallocating.
        signatures.push_back(Signature{nullptr, posting.back(), nullptr});
      }

      /// Declare `ident` with `args` in MiniZinc's argument order, to be posted
      /// as `posts_as` with the arguments reordered: the poster's argument \a i
      /// is the declared argument `perm[i]`.
      void addPermuted(std::string ident, std::string posts_as,
                       std::initializer_list<FznsoType> args,
                       std::initializer_list<std::size_t> perm) {
        addVector(std::move(ident), std::move(posts_as), std::vector<FznsoType>(args));
        permutations.push_back(std::vector<std::size_t>(perm));
        signatures.back().permutation = &permutations.back();
      }

      /// Point each signature at its now-stable constraint type.
      void link() {
        for (std::size_t i = 0; i < signatures.size(); i++)
          signatures[i].declared = &types[i];
      }

      std::deque<std::string> names;
      std::deque<std::string> posting;
      std::deque<std::vector<FznsoType> > arguments;
      std::deque<std::vector<std::size_t> > permutations;
      std::vector<FznsoConstraintType> types;
      std::vector<Signature> signatures;
      std::map<std::string, std::size_t> index;
    };

    const Table& table() {
      static const Table t = [] {
        Table t;

        // --- integer relations -------------------------------------------
        for (const char* n : {"int_eq", "int_ne", "int_ge", "int_gt", "int_le", "int_lt"}) {
          t.addReified(n, {VI, VI});
        }

        // --- integer linear relations ------------------------------------
        for (const char* n : {"int_lin_eq", "int_lin_ne", "int_lin_ge", "int_lin_gt",
                              "int_lin_le", "int_lin_lt"}) {
          t.addReified(n, {AI, AVI, I});
        }

        // --- integer arithmetic ------------------------------------------
        for (const char* n : {"int_plus", "int_minus", "int_times", "int_div", "int_mod",
                              "int_min", "int_max"}) {
          t.add(n, {VI, VI, VI});
        }
        t.add("int_abs", {VI, VI});
        t.add("int_negate", {VI, VI});
        t.addGecode("int_pow", {VI, I, VI});

        // --- Boolean relations -------------------------------------------
        for (const char* n : {"bool_eq", "bool_ne", "bool_ge", "bool_gt", "bool_le", "bool_lt"}) {
          t.addReified(n, {VB, VB});
        }
        for (const char* n : {"bool_and", "bool_or", "bool_xor", "bool_and_imp", "bool_or_imp",
                              "bool_xor_imp", "bool_left_imp", "bool_right_imp"}) {
          t.add(n, {VB, VB, VB});
        }
        t.add("bool_not", {VB, VB});
        t.add("array_bool_and", {AVB, VB});
        t.add("array_bool_and_imp", {AVB, VB});
        t.add("array_bool_or", {AVB, VB});
        t.add("array_bool_or_imp", {AVB, VB});
        // The standard builtin takes only the array; Gecode also accepts a
        // trailing `var bool`, which `argument_type` types from the last entry.
        t.add("array_bool_xor", {AVB});
        t.add("array_bool_xor_imp", {AVB, VB});
        t.add("bool_clause", {AVB, AVB});
        t.add("bool_clause_reif", {AVB, AVB, VB});
        t.add("bool_clause_imp", {AVB, AVB, VB});
        t.add("bool2int", {VB, VI});

        // --- Boolean linear relations ------------------------------------
        for (const char* n : {"bool_lin_eq", "bool_lin_ne", "bool_lin_ge", "bool_lin_gt",
                              "bool_lin_le", "bool_lin_lt"}) {
          t.addReified(n, {AI, AVB, VI});
        }

        // --- element -----------------------------------------------------
        t.add("array_int_element", {VI, AI, VI});
        t.add("array_var_int_element", {VI, AVI, VI});
        t.addGecode("int_element", {VI, I, AVI, VI});
        t.addGecode("var_int_element", {VI, I, AVI, VI});
        t.addGecode("int_element2d", {VI, VI, AVI, S, S, VI});
        t.add("array_bool_element", {VI, AB, VB});
        t.add("array_var_bool_element", {VI, AVB, VB});
        t.addGecode("bool_element", {VI, I, AVB, VB});
        t.addGecode("var_bool_element", {VI, I, AVB, VB});
        t.addGecode("bool_element2d", {VI, VI, AVB, S, S, VB});

        // --- domain membership -------------------------------------------
        t.add("int_in", {VI, S});
        t.add("int_in_reif", {VI, S, VB});
        t.add("int_in_imp", {VI, S, VB});
        t.add("set_in", {VI, VS});
        t.add("set_in_reif", {VI, VS, VB});
        t.add("set_in_imp", {VI, VS, VB});
        // MiniZinc spells membership in a fixed set `set_in` as well as
        // `int_in`, and only the former has a half-reified form to lower to.
        t.add("set_in", {VI, S});
        t.add("set_in_reif", {VI, S, VB});
        t.add("set_in_imp", {VI, S, VB});

        // --- array ordering ----------------------------------------------
        t.add("array_int_lt", {AVI, AVI});
        // A consumer asks for a constraint by the name its own interface uses,
        // so these are declared under MiniZinc's names as well; the poster is
        // unchanged.
        t.addVector("lex_less_int", "array_int_lt", {AVI, AVI});
        t.add("array_int_lq", {AVI, AVI});
        t.addVector("lex_lesseq_int", "array_int_lq", {AVI, AVI});
        t.add("array_bool_lt", {AVB, AVB});
        t.addVector("lex_less_bool", "array_bool_lt", {AVB, AVB});
        t.add("array_bool_lq", {AVB, AVB});
        t.addVector("lex_lesseq_bool", "array_bool_lq", {AVB, AVB});
        t.add("increasing_int", {AVI});
        t.add("increasing_bool", {AVB});
        // No `decreasing`: MiniZinc reads it as `increasing` over the reversed
        // array, so there is nothing for a declaration of it to match.
        t.add("sort", {AVI, AVI});

        // --- counting ----------------------------------------------------
        for (const char* n : {"count", "count_eq"}) {
          t.addReified(n, {AVI, VI, VI});
        }
        t.add("at_least_int", {I, AVI, I});
        t.add("at_most_int", {I, AVI, I});
        t.add("nvalue", {VI, AVI});
        t.add("among", {VI, AVI, S});
        t.add("member_int", {AVI, VI});
        t.addGecode("member_int_reif", {AVI, VI, VB});
        t.add("member_bool", {AVB, VB});
        t.addGecode("member_bool_reif", {AVB, VB, VB});
        t.add("global_cardinality_low_up", {AVI, AI, AI, AI});
        t.add("global_cardinality_low_up_closed", {AVI, AI, AI, AI});
        t.addGecode("global_cardinality", {AVI, AI, AVI});
        t.addGecode("global_cardinality_closed", {AVI, AI, AVI});

        // --- extrema -----------------------------------------------------
        t.add("array_int_minimum", {VI, AVI});
        t.add("array_int_maximum", {VI, AVI});
        // The offset is the index the caller numbers the array from, which a
        // FlatZinc array cannot carry; the `_offset` in the posting name says
        // as much, but the constraint is the one MiniZinc calls `*_arg_*`.
        t.addVector("minimum_arg_int", "gecode_minimum_arg_int_offset", {AVI, I, VI});
        t.addVector("maximum_arg_int", "gecode_maximum_arg_int_offset", {AVI, I, VI});
        t.addVector("minimum_arg_bool", "gecode_minimum_arg_bool_offset", {AVB, I, VI});
        t.addVector("maximum_arg_bool", "gecode_maximum_arg_bool_offset", {AVB, I, VI});

        // --- distinctness and channelling --------------------------------
        t.add("all_different_int", {AVI});
        t.add("all_different_offset", {AI, AVI});
        t.add("all_equal_int", {AVI});
        // Each array is followed by the index it is numbered from; the poster
        // instead pairs each array with the *other* one's offset, since that is
        // what its values are read against.
        t.addPermuted("inverse", "inverse_offsets", {AVI, I, AVI, I}, {0, 3, 2, 1});

        // --- extensional -------------------------------------------------
        t.addGecode("regular", {AVI, I, I, AI, I, S});
        t.addGecode("regular_set", {AVI, I, I, I, AI, I, S});
        t.addGecodeReified("table_int", {AVI, AI});
        t.addGecodeReified("table_bool", {AVB, AB});
        t.addGecode("among_seq_int", {AVI, S, I, I, I});
        t.addGecode("among_seq_bool", {AVB, B, I, I, I});
        // MiniZinc names the bounds and the window first and the array last;
        // the poster reads the array, the values, then the window and bounds.
        t.addPermuted("sliding_among_int", "gecode_among_seq_int",
                      {I, I, I, AVI, S}, {3, 4, 2, 0, 1});
        t.addPermuted("sliding_among_bool", "gecode_among_seq_bool",
                      {I, I, I, AVB, B}, {3, 4, 2, 0, 1});

        // --- scheduling and packing --------------------------------------
        // One machine of a fixed capacity: the same poster, told apart by the
        // number of arguments.
        t.addVector("cumulative", "cumulatives", {AVI, AVI, AVI, VI});
        // `machine` is followed by the number its values are counted from,
        // which the propagator takes last and shifts its machines to zero by.
        t.addPermuted("cumulatives", "cumulatives",
                      {AVI, AVI, AVI, AVI, I, AI, B}, {0, 1, 2, 3, 5, 6, 4});
        t.addGecode("schedule_unary", {AVI, AI});
        // Unary scheduling is disjunctive with fixed durations, which a
        // consumer reaches by declaring the fixed form of the constraint.
        t.addVector("disjunctive_strict", "gecode_schedule_unary", {AVI, AI});
        t.addGecode("schedule_unary_optional", {AVI, AI, AVB});
        t.addGecode("schedule_cumulative_optional", {AVI, AI, AI, AVB, I});
        t.addGecode("nooverlap", {AVI, AVI, AVI, AVI});
        t.addVector("no_overlap", "gecode_nooverlap", {AVI, AVI, AVI, AVI});
        // Each array is followed by the index it is numbered from, which for
        // the poster is a trailing argument.
        t.addPermuted("bin_packing_load", "gecode_bin_packing_load",
                      {AVI, I, AVI, AI}, {0, 2, 3, 1});

        // --- circuit -----------------------------------------------------
        t.addGecode("circuit", {I, AVI});
        // Declared under the name they post as, not the bare one: these take
        // successors already shifted to be zero-based, so the bare name belongs
        // to the wrapper in `gecode.mzn` that does the shifting.
        t.add("gecode_circuit_cost", {AI, AVI, VI});
        t.add("gecode_circuit_cost_array", {AI, AVI, AVI, VI});
        t.addGecode("precede", {AVI, I, I});
        t.addPermuted("value_precede_int", "gecode_precede",
                      {I, I, AVI}, {2, 0, 1});

        // --- restart-based ---------------------------------------------
        // Recorded rather than posted: `postConstraints` collects these and
        // builds the arrays the search reads them from.
        t.add("on_restart_status", {VI});
        t.add("on_restart_complete", {VB});
        t.add("on_restart_sol_int", {VI, VI});
        t.add("on_restart_sol_bool", {VB, VB});
        t.add("on_restart_last_val_int", {VI, VI});
        t.add("on_restart_last_val_bool", {VB, VB});
        t.add("on_restart_uniform_int", {I, I, VI});

        // --- black box ---------------------------------------------------
        t.addGecode("blackbox", {AVI, AVF, AVI, AVF});
        t.addGecode("blackbox_bounds", {AVI, AVF, AI});

#ifdef GECODE_HAS_SET_VARS
        // --- set relations -----------------------------------------------
        for (const char* n : {"set_eq", "set_ne", "set_le", "set_lt", "set_subset",
                              "set_superset", "equal"}) {
          t.add(n, {VS, VS});
          t.add(std::string(n) + "_reif", {VS, VS, VB});
        }
        t.add("disjoint", {VS, VS});
        t.add("set_convex", {VS});
        t.add("set_card", {VS, VI});
        for (const char* n : {"set_union", "set_intersect", "set_diff", "set_symdiff"}) {
          t.add(n, {VS, VS, VS});
        }
        t.add("array_set_element", {VI, AS, VS});
        t.add("array_var_set_element", {VI, AVS, VS});
        t.add("array_set_union", {AVS, VS});
        t.add("array_set_partition", {AVS, VS});
        // MiniZinc's `fzn_partition_set` fixes the universe, and `arg2SetVar`
        // takes a set literal as readily as a variable, so the alias is
        // declared with the argument the caller actually passes.
        t.addVector("partition_set", "array_set_partition", {AVS, S});
        t.add("array_set_seq", {AVS});
        t.add("array_set_seq_union", {AVS, VS});
        t.addGecode("array_set_element_union", {VS, AVS, VS});
        t.addGecode("array_set_element_intersect", {VS, AVS, VS});
        t.addGecode("array_set_element_intersect_in", {VS, AVS, VS, S});
        t.addGecode("array_set_element_partition", {VS, AVS, VS});
        t.addPermuted("inverse_set", "gecode_inverse_set",
                      {AVS, I, AVS, I}, {0, 2, 1, 3});
        t.addGecode("precede_set", {AVS, I, I});
        t.addPermuted("value_precede_set", "gecode_precede_set",
                      {I, I, AVS}, {2, 0, 1});
        t.addGecode("int_set_channel", {AVI, I, AVS, I});
        t.addGecode("link_set_to_booleans", {VS, AVB, I});
        t.addGecode("range", {AVI, I, VS, VS});
        t.addGecode("set_weights", {AI, AI, VS, VI});
        t.add("on_restart_sol_set", {VS, VS});
        t.add("on_restart_last_val_set", {VS, VS});
        t.addVector("sum_set", "gecode_set_weights", {AI, AI, VS, VI});
#endif

#ifdef GECODE_HAS_FLOAT_VARS
        // --- float relations and arithmetic ------------------------------
        t.add("int2float", {VI, VF});
        t.add("on_restart_sol_float", {VF, VF});
        t.add("on_restart_last_val_float", {VF, VF});
        t.add("on_restart_uniform_float", {F, F, VF});
        t.add("float_abs", {VF, VF});
        t.add("float_sqrt", {VF, VF});
        t.add("float_ne", {VF, VF});
        for (const char* n : {"float_eq", "float_le", "float_lt"}) {
          t.add(n, {VF, VF});
          t.add(std::string(n) + "_reif", {VF, VF, VB});
        }
        for (const char* n : {"float_plus", "float_times", "float_div", "float_min",
                              "float_max"}) {
          t.add(n, {VF, VF, VF});
        }
        for (const char* n : {"float_lin_eq", "float_lin_le", "float_lin_lt"}) {
          t.add(n, {AF, AVF, F});
          t.add(std::string(n) + "_reif", {AF, AVF, F, VB});
        }
#ifdef GECODE_HAS_MPFR
        // Transcendentals need MPFR, exactly as their registrations do.
        for (const char* n : {"float_acos", "float_asin", "float_atan", "float_cos",
                              "float_cosh", "float_exp", "float_ln", "float_log10",
                              "float_log2", "float_sin", "float_sinh", "float_tan",
                              "float_tanh"}) {
          t.add(n, {VF, VF});
        }
#endif
#endif
        t.link();
        return t;
      }();
      return t;
    }

  }

  FznsoConstraintList GecodeSolver::constraint_list() {
    const Table& t = table();
    return FznsoConstraintList{t.types.size(), t.types.data()};
  }

  const Signature* signature(std::string_view ident) {
    const Table& t = table();
    std::map<std::string, std::size_t>::const_iterator i =
      t.index.find(std::string(ident));
    if (i != t.index.end())
      return &t.signatures[i->second];
    // `Registry::add` registers every constraint under a `fzn_` and a `gecode_`
    // prefix as well as its own name, and a MiniZinc library calls the
    // prefixed spellings. The declared list keeps only the bare name, so a
    // prefixed one is resolved by stripping it.
    for (std::string_view prefix : {std::string_view("fzn_"),
                                    std::string_view("gecode_")}) {
      if (ident.size() > prefix.size() &&
          ident.compare(0, prefix.size(), prefix) == 0) {
        i = t.index.find(std::string(ident.substr(prefix.size())));
        if (i != t.index.end())
          return &t.signatures[i->second];
      }
    }
    return nullptr;
  }

  const FznsoType& argument_type(const FznsoConstraintType& sig, std::size_t index) {
    if (sig.arg_len == 0) {
      // No declared constraint is nullary, but a caller must still get
      // something readable back rather than an out-of-bounds argument type.
      static const FznsoType any = VI;
      return any;
    }
    return sig.arg_types[index < sig.arg_len ? index : sig.arg_len - 1];
  }

  FznsoTypeList GecodeSolver::decision_list() {
    static const FznsoType types[] = {
      VB, VI,
#ifdef GECODE_HAS_FLOAT_VARS
      VF,
#endif
#ifdef GECODE_HAS_SET_VARS
      VS,
#endif
    };
    return FznsoTypeList{sizeof(types)/sizeof(types[0]), types};
  }

  FznsoObjectiveList GecodeSolver::objective_list() {
    static const FznsoObjective objectives[] = {
      {str("minimize_int"), VI},
      {str("maximize_int"), VI},
#ifdef GECODE_HAS_FLOAT_VARS
      {str("minimize_float"), VF},
      {str("maximize_float"), VF},
#endif
    };
    return FznsoObjectiveList{sizeof(objectives)/sizeof(objectives[0]), objectives};
  }

  namespace {

    /// An optional integer: absent means "no limit".
    constexpr FznsoType OPT_I = fznso::Type{FznsoTypeBaseInt}.opt(true);
    constexpr FznsoType STR = fznso::Type{FznsoTypeBaseString};

    /// Storage the default values in `option_list` borrow. A `fznso::Value`
    /// points at its payload rather than copying it, so these must outlive
    /// every use of the list — which, being `static`, they do.
    struct Defaults {
      bool no = false;
      std::int64_t one = 1;
      std::int64_t zero = 0;
      std::int64_t c_d = Search::Config::c_d;
      std::int64_t a_d = Search::Config::a_d;
      std::int64_t nogoods_limit = Search::Config::nogoods_limit;
      std::int64_t restart_scale = 250;
      double restart_base = 1.5;
      double decay = 0.99;
      double step = 0.0;
      std::string none = "none";
    };
    const Defaults& defaults() {
      static const Defaults d;
      return d;
    }

  }

  FznsoOptionList GecodeSolver::option_list() {
    static const FznsoOption options[] = {
      // The common options every FZnSO solver is encouraged to accept.
      {str("all_solutions"), B,
       fznso::Value{defaults().no}.raw()},
      {str("fixed_search"), B,
       fznso::Value{defaults().no}.raw()},
      {str("intermediate"), B,
       fznso::Value{defaults().no}.raw()},
      {str("threads"), I, fznso::Value{defaults().one}.raw()},
      {str("time_limit"), OPT_I, fznso::Value{}.raw()},
      {str("random_seed"), OPT_I, fznso::Value{}.raw()},
      {str("verbose"), B,
       fznso::Value{defaults().no}.raw()},
      // Gecode's own knobs, named after the FlatZinc interpreter's flags.
      {str("solution_limit"), I, fznso::Value{defaults().zero}.raw()},
      {str("node_limit"), OPT_I, fznso::Value{}.raw()},
      {str("fail_limit"), OPT_I, fznso::Value{}.raw()},
      {str("restart"), STR, fznso::Value{defaults().none}.raw()},
      {str("restart_base"), F, fznso::Value{defaults().restart_base}.raw()},
      {str("restart_scale"), I, fznso::Value{defaults().restart_scale}.raw()},
      {str("restart_limit"), OPT_I, fznso::Value{}.raw()},
      {str("nogoods"), B,
       fznso::Value{defaults().no}.raw()},
      {str("nogoods_limit"), I, fznso::Value{defaults().nogoods_limit}.raw()},
      {str("c_d"), I, fznso::Value{defaults().c_d}.raw()},
      {str("a_d"), I, fznso::Value{defaults().a_d}.raw()},
      {str("decay"), F, fznso::Value{defaults().decay}.raw()},
      {str("float_step"), F, fznso::Value{defaults().step}.raw()},
    };
    return FznsoOptionList{sizeof(options)/sizeof(options[0]), options};
  }

  FznsoStatisticList GecodeSolver::statistic_list() {
    // The search counters can be read while a run is in progress, so they are
    // available from a solution as well as from the solver; the timings and the
    // model sizes only make sense once a run has finished.
    static const FznsoStatistic stats[] = {
      {str("solutions"), I, true, true},
      {str("nodes"), I, true, true},
      {str("failures"), I, true, true},
      {str("propagations"), I, true, true},
      {str("restarts"), I, true, true},
      {str("peak_depth"), I, true, true},
      {str("variables"), I, false, true},
      {str("propagators"), I, false, true},
      {str("init_time"), F, false, true},
      {str("solve_time"), F, false, true},
    };
    return FznsoStatisticList{sizeof(stats)/sizeof(stats[0]), stats};
  }

}}

// STATISTICS: flatzinc-any
