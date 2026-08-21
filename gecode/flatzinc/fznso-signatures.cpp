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

        // --- integer linear relations ------------------------------------
        // The registry has no `int_lt`, `int_ge` or `int_plus`: a comparison of
        // two integers is a linear constraint over two terms, and that is the
        // only spelling it defines. `redefinitions.mzn` rewrites each of them
        // that way, so declaring them here would take the rewrite away and post
        // a name the specification does not have.
        for (const char* n : {"int_lin_eq", "int_lin_ne", "int_lin_le"}) {
          t.addReified(n, {AI, AVI, I});
        }

        // --- integer arithmetic ------------------------------------------
        // `int_plus`, `int_minus`, `int_min`, `int_max` and `int_negate` go the
        // same way: linear constraints, or the array extrema.
        for (const char* n : {"int_times", "int_div", "int_mod"}) {
          t.add(n, {VI, VI, VI});
        }
        t.add("int_abs", {VI, VI});
        t.addGecode("int_pow", {VI, I, VI});

        // --- Boolean relations -------------------------------------------
        // A comparison of two Booleans is a clause, a conjunction or disjunction
        // of two is the array form with a two-element array, and a negation is
        // `bool_array_xor` over the two, so the registry names none of
        // `bool_eq`, `bool_le`, `bool_and`, `bool_or`, `bool_not` or
        // `array_bool_or`.
        t.addVector("bool_array_and", "array_bool_and", {AVB, VB});
        t.addVector("bool_array_xor", "array_bool_xor", {AVB});
        t.addVector("bool_array_xor_imp", "array_bool_xor_imp", {AVB, VB});
        t.add("bool_clause", {AVB, AVB});
        t.add("bool_clause_reif", {AVB, AVB, VB});
        t.addVector("bool_to_int", "bool2int", {VB, VI});

        // --- Boolean linear relations ------------------------------------
        t.addVector("bool_lin_eq", "bool_lin_eq", {AI, AVB, VI});
        t.addReified("bool_lin_ne", {AI, AVB, VI});
        // The bound of an inequality is a constant: that is what the registry
        // defines and what `flatzinc_builtins.mzn` declares. The poster takes a
        // variable too, but declaring one would be wider than the registry, and
        // nothing may declare wider than it means.
        t.addReified("bool_lin_le", {AI, AVB, I});

        // --- element -----------------------------------------------------
        t.addPermuted("int_array_element", "gecode_int_element", {AVI, I, VI, VI}, {2, 1, 0, 3});
        t.add("gecode_int_element", {VI, I, AVI, VI});
        t.add("gecode_var_int_element", {VI, I, AVI, VI});
        t.add("gecode_int_element2d", {VI, VI, AVI, S, S, VI});
        t.addPermuted("bool_array_element", "gecode_bool_element", {AVB, I, VI, VB}, {2, 1, 0, 3});
        t.add("gecode_bool_element", {VI, I, AVB, VB});
        t.add("gecode_var_bool_element", {VI, I, AVB, VB});
        t.add("gecode_bool_element2d", {VI, VI, AVB, S, S, VB});

        // --- domain membership -------------------------------------------
        // Gecode has an `int_in` poster of its own for a fixed set, but the
        // registry's `int_in` is membership of any set and `set_in` is the
        // poster that takes both. Only the first declaration of a name is
        // indexed, so declaring Gecode's as well would type a variable set as a
        // literal and the poster would reject it.
        t.addVector("int_in", "set_in", {VI, VS});
        t.addVector("int_in_reif", "set_in_reif", {VI, VS, VB});
        t.addVector("int_in_imp", "set_in_imp", {VI, VS, VB});
        // MiniZinc spells membership in a fixed set `set_in` as well as
        // `int_in`, and only the former has a half-reified form to lower to.
        t.addVector("int_in", "set_in", {VI, S});
        t.addVector("int_in_reif", "set_in_reif", {VI, S, VB});
        t.addVector("int_in_imp", "set_in_imp", {VI, S, VB});

        // --- array ordering ----------------------------------------------
        t.addVector("gecode_array_int_lt", "array_int_lt", {AVI, AVI});
        // A consumer asks for a constraint by the name the FZnSO registry gives
        // it, so each of these is declared under that name as well as Gecode's
        // own; the poster is unchanged either way.
        t.addVector("int_lex_less", "array_int_lt", {AVI, AVI});
        t.addVector("gecode_array_int_lq", "array_int_lq", {AVI, AVI});
        t.addVector("int_lex_lesseq", "array_int_lq", {AVI, AVI});
        t.addVector("gecode_array_bool_lt", "array_bool_lt", {AVB, AVB});
        t.addVector("bool_lex_less", "array_bool_lt", {AVB, AVB});
        t.addVector("gecode_array_bool_lq", "array_bool_lq", {AVB, AVB});
        t.addVector("bool_lex_lesseq", "array_bool_lq", {AVB, AVB});
        t.addVector("int_increasing", "increasing_int", {AVI});
        t.addVector("bool_increasing", "increasing_bool", {AVB});
        // No `decreasing`: MiniZinc reads it as `increasing` over the reversed
        // array, so there is nothing for a declaration of it to match.
        t.addVector("int_array_sort", "sort", {AVI, AVI});

        // --- counting ----------------------------------------------------
        t.addVector("int_count", "count_eq", {AVI, VI, VI});
        t.addPermuted("int_nvalue", "nvalue", {AVI, VI}, {1, 0});
        t.addPermuted("int_among", "among", {AVI, S, VI}, {2, 0, 1});
        t.addVector("int_array_member", "member_int", {AVI, VI});
        t.addVector("int_array_member_reif", "gecode_member_int_reif", {AVI, VI, VB});
        t.addVector("bool_array_member", "member_bool", {AVB, VB});
        t.addVector("bool_array_member_reif", "gecode_member_bool_reif", {AVB, VB, VB});
        t.addVector("int_global_cardinality_low_up", "global_cardinality_low_up",
                    {AVI, AI, AI, AI});
        t.addVector("int_global_cardinality_low_up_closed", "global_cardinality_low_up_closed",
                    {AVI, AI, AI, AI});
        t.addVector("int_global_cardinality", "gecode_global_cardinality", {AVI, AI, AVI});
        t.addVector("int_global_cardinality_closed", "gecode_global_cardinality_closed",
                    {AVI, AI, AVI});

        // --- extrema -----------------------------------------------------
        // The registry puts the result last; the poster takes it first.
        t.addPermuted("int_array_minimum", "array_int_minimum", {AVI, VI}, {1, 0});
        t.addPermuted("int_array_maximum", "array_int_maximum", {AVI, VI}, {1, 0});
        // The offset is the index the caller numbers the array from, which a
        // FlatZinc array cannot carry; the `_offset` in the posting name says
        // as much, but the constraint is the one MiniZinc calls `*_arg_*`.
        t.addVector("int_array_arg_min", "gecode_minimum_arg_int_offset", {AVI, I, VI});
        t.addVector("int_array_arg_max", "gecode_maximum_arg_int_offset", {AVI, I, VI});
        t.addVector("bool_array_arg_min", "gecode_minimum_arg_bool_offset", {AVB, I, VI});
        t.addVector("bool_array_arg_max", "gecode_maximum_arg_bool_offset", {AVB, I, VI});

        // --- distinctness and channelling --------------------------------
        t.addVector("int_all_different", "all_different_int", {AVI});
        t.addVector("gecode_all_different_offset", "all_different_offset", {AI, AVI});
        t.addVector("int_all_equal", "all_equal_int", {AVI});
        // Each array is followed by the index it is numbered from; the poster
        // instead pairs each array with the *other* one's offset, since that is
        // what its values are read against.
        t.addPermuted("int_inverse", "inverse_offsets", {AVI, I, AVI, I}, {0, 3, 2, 1});

        // --- extensional -------------------------------------------------
        // The registry numbers the symbols from an offset; `gecode_regular`
        // assumes they start at 1, so the offset-taking poster is the match.
        t.addVector("int_regular", "gecode_regular_offset", {AVI, I, I, I, AI, I, S});
        t.add("gecode_regular_set", {AVI, I, I, I, AI, I, S});
        // Gecode registers these under its own spelling, which the registry
        // rename no longer derives, so each posting name is given explicitly.
        t.addVector("int_table", "gecode_table_int", {AVI, AI});
        t.addVector("int_table_reif", "gecode_table_int_reif", {AVI, AI, VB});
        t.addVector("int_table_imp", "gecode_table_int_imp", {AVI, AI, VB});
        t.addVector("bool_table", "gecode_table_bool", {AVB, AB});
        t.addVector("bool_table_reif", "gecode_table_bool_reif", {AVB, AB, VB});
        t.addVector("bool_table_imp", "gecode_table_bool_imp", {AVB, AB, VB});
        t.add("gecode_among_seq_int", {AVI, S, I, I, I});
        t.add("gecode_among_seq_bool", {AVB, B, I, I, I});
        // The registry opens with the array, as the poster does, so unlike
        // MiniZinc's spelling — which names the bounds and the window first —
        // there is nothing to permute.
        t.addVector("int_sliding_among", "gecode_among_seq_int", {AVI, S, I, I, I});
        t.addVector("bool_sliding_among", "gecode_among_seq_bool", {AVB, B, I, I, I});

        // --- scheduling and packing --------------------------------------
        // One machine of a fixed capacity: the same poster, told apart by the
        // number of arguments.
        t.addVector("int_cumulative", "cumulatives", {AVI, AVI, AVI, VI});
        // `machine` is followed by the number its values are counted from,
        // which the propagator takes last and shifts its machines to zero by.
        t.addPermuted("int_cumulatives", "cumulatives",
                      {AVI, AVI, AVI, AVI, AI, I, B}, {0, 1, 2, 3, 4, 6, 5});
        t.add("gecode_schedule_unary", {AVI, AI});
        // Unary scheduling is disjunctive with fixed durations, which a
        // consumer reaches by declaring the fixed form of the constraint.
        t.addVector("int_disjunctive_strict", "gecode_schedule_unary", {AVI, AI});
        t.add("gecode_schedule_unary_optional", {AVI, AI, AVB});
        t.add("gecode_schedule_cumulative_optional", {AVI, AI, AI, AVB, I});
        t.add("gecode_nooverlap", {AVI, AVI, AVI, AVI});
        t.addVector("int_no_overlap", "gecode_nooverlap", {AVI, AVI, AVI, AVI});
        // The registry reads items then bins, `load` followed by the index it
        // is numbered from; the poster takes the loads first, the offset last.
        t.addPermuted("int_bin_packing_load", "gecode_bin_packing_load",
                      {AVI, AI, AVI, I}, {2, 0, 1, 3});

        // --- circuit -----------------------------------------------------
        t.addPermuted("int_circuit", "gecode_circuit", {AVI, I}, {1, 0});
        // Declared under the name they post as, not the bare one: these take
        // successors already shifted to be zero-based, so the bare name belongs
        // to the wrapper in `gecode.mzn` that does the shifting.
        t.add("gecode_circuit_cost", {AI, AVI, VI});
        t.add("gecode_circuit_cost_array", {AI, AVI, AVI, VI});
        t.add("gecode_precede", {AVI, I, I});
        t.addPermuted("int_value_precede", "gecode_precede",
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
        // `set_superset` has no entry of its own: it is `set_of_int_subset`
        // with its arguments the other way round, which the rewrite layer does.
        for (const char* n : {"eq", "ne", "le", "lt", "subset"}) {
          t.addVector("set_of_int_" + std::string(n), "set_" + std::string(n), {VS, VS});
          t.addVector("set_of_int_" + std::string(n) + "_reif", "set_" + std::string(n) + "_reif",
                      {VS, VS, VB});
        }
        t.addVector("gecode_equal", "equal", {VS, VS});
        t.addVector("gecode_equal_reif", "equal_reif", {VS, VS, VB});
        t.addVector("gecode_disjoint", "disjoint", {VS, VS});
        t.addVector("gecode_set_convex", "set_convex", {VS});
        t.addVector("set_of_int_card", "set_card", {VS, VI});
        for (const char* n : {"union", "intersect", "diff", "symdiff"}) {
          t.addVector("set_of_int_" + std::string(n), "set_" + std::string(n), {VS, VS, VS});
        }
        // The registry pairs an array with the number it is indexed from, and
        // Gecode's set element propagator is one-based with no argument for
        // that. A permutation could drop the offset, but the declaration would
        // then be wider than the propagator means — a caller numbering its
        // array from anything else would be answered wrongly and silently. So
        // the registry name is left undeclared and the propagator is exposed
        // under a name of Gecode's own, for `mznlib-fznso` to shift the index
        // into. That file must not name `array_var_set_element` itself:
        // `redefinitions.mzn` rewrites that builtin back onto the registry
        // identifier, which is the file's own head, and the flattener loops.
        t.addVector("gecode_set_element", "array_var_set_element", {VI, AVS, VS});
        t.addVector("set_of_int_array_union", "array_set_union", {AVS, VS});
        t.addVector("gecode_array_set_partition", "array_set_partition", {AVS, VS});
        // MiniZinc's `fzn_partition_set` fixes the universe, and `arg2SetVar`
        // takes a set literal as readily as a variable, so the alias is
        // declared with the argument the caller actually passes.
        t.addVector("set_of_int_partition", "array_set_partition", {AVS, S});
        t.addVector("gecode_array_set_seq", "array_set_seq", {AVS});
        t.addVector("gecode_array_set_seq_union", "array_set_seq_union", {AVS, VS});
        t.add("gecode_array_set_element_union", {VS, AVS, VS});
        t.add("gecode_array_set_element_intersect", {VS, AVS, VS});
        t.add("gecode_array_set_element_intersect_in", {VS, AVS, VS, S});
        t.add("gecode_array_set_element_partition", {VS, AVS, VS});
        t.addPermuted("set_of_int_inverse", "gecode_inverse_set",
                      {AVS, I, AVS, I}, {0, 2, 1, 3});
        t.add("gecode_precede_set", {AVS, I, I});
        t.addPermuted("set_of_int_value_precede", "gecode_precede_set",
                      {I, I, AVS}, {2, 0, 1});
        t.addGecode("int_set_channel", {AVI, I, AVS, I});
        t.addVector("set_of_int_to_bool", "gecode_link_set_to_booleans", {VS, AVB, I});
        t.addVector("int_array_range", "gecode_range", {AVI, I, VS, VS});
        t.add("gecode_set_weights", {AI, AI, VS, VI});
        t.add("on_restart_sol_set", {VS, VS});
        t.add("on_restart_last_val_set", {VS, VS});
        t.addVector("gecode_sum_set", "gecode_set_weights", {AI, AI, VS, VI});
#endif

#ifdef GECODE_HAS_FLOAT_VARS
        // --- float relations and arithmetic ------------------------------
        t.addVector("int_to_float", "int2float", {VI, VF});
        t.add("on_restart_sol_float", {VF, VF});
        t.add("on_restart_last_val_float", {VF, VF});
        t.add("on_restart_uniform_float", {F, F, VF});
        t.add("float_abs", {VF, VF});
        t.add("float_sqrt", {VF, VF});
        // The registry names no binary float comparison and no `float_plus`:
        // each is a linear constraint over one or two terms, and that is its
        // only spelling. `p_float_lin_cmp` recognises those shapes and posts
        // the binary propagator, so nothing is lost by leaving them out —
        // which matters here, because `Float::linear` fails with "Number out
        // of limits" on an unbounded operand.
        for (const char* n : {"float_times", "float_div"}) {
          t.add(n, {VF, VF, VF});
        }
        // Gecode has no array extremum over floats, so the chain of the binary
        // one is written in `mznlib-fznso`. That file cannot name `float_max`:
        // `redefinitions.mzn` rewrites that builtin onto `fzn_float_array_-
        // maximum`, which is the file's own head, and the flattener would not
        // terminate. The propagator is exposed under a name of Gecode's own
        // instead, which nothing rewrites.
        t.addVector("gecode_float_max", "float_max", {VF, VF, VF});
        t.addVector("gecode_float_min", "float_min", {VF, VF, VF});
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
      {str("int_minimize"), VI},
      {str("int_maximize"), VI},
#ifdef GECODE_HAS_FLOAT_VARS
      {str("float_minimize"), VF},
      {str("float_maximize"), VF},
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
      {str("gecode_solution_limit"), I, fznso::Value{defaults().zero}.raw()},
      {str("gecode_node_limit"), OPT_I, fznso::Value{}.raw()},
      {str("gecode_fail_limit"), OPT_I, fznso::Value{}.raw()},
      {str("gecode_restart"), STR, fznso::Value{defaults().none}.raw()},
      {str("gecode_restart_base"), F, fznso::Value{defaults().restart_base}.raw()},
      {str("gecode_restart_scale"), I, fznso::Value{defaults().restart_scale}.raw()},
      {str("gecode_restart_limit"), OPT_I, fznso::Value{}.raw()},
      {str("gecode_nogoods"), B,
       fznso::Value{defaults().no}.raw()},
      {str("gecode_nogoods_limit"), I, fznso::Value{defaults().nogoods_limit}.raw()},
      {str("gecode_c_d"), I, fznso::Value{defaults().c_d}.raw()},
      {str("gecode_a_d"), I, fznso::Value{defaults().a_d}.raw()},
      {str("gecode_decay"), F, fznso::Value{defaults().decay}.raw()},
      {str("gecode_float_step"), F, fznso::Value{defaults().step}.raw()},
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
      {str("decisions"), I, false, true},
      {str("propagators"), I, false, true},
      {str("init_time"), F, false, true},
      {str("solve_time"), F, false, true},
    };
    return FznsoStatisticList{sizeof(stats)/sizeof(stats[0]), stats};
  }

}}

// STATISTICS: flatzinc-any
