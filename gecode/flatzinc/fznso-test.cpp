/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  End-to-end checks for Gecode behind the FZnSO protocol.
 *
 *  The solver is loaded the way a real consumer loads it — through
 *  `fznso::Library`, from the built shared object — so the entry-point naming
 *  and the export path are exercised along with the translation itself.
 *
 *  Usage: fznso-gecode-test <path to libgecode.dylib|.so|.dll>
 */

#include <gecode/flatzinc/fznso.hh>
#include <gecode/flatzinc/registry.hh>

#include <gecode/third-party/fznso/fznso.hpp>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace {

using fznso::Decision;
using fznso::LayeredModel;
using fznso::OwnedValue;
using fznso::Solution;
using fznso::Status;

/// The decision types these fixtures use.
constexpr FznsoType BOOL = fznso::Type{FznsoTypeBaseBool}.decision(true);
constexpr FznsoType INT = fznso::Type{FznsoTypeBaseInt}.decision(true);
constexpr FznsoType FLOAT = fznso::Type{FznsoTypeBaseFloat}.decision(true);
constexpr FznsoType SET = fznso::Type{FznsoTypeBaseInt}.decision(true).set(true);

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", what);
    failures++;
  }
}

/// One run's answers, collected so the assertions can look at them afterwards.
struct Collected {
  std::vector<std::vector<std::int64_t> > solutions;
  std::vector<std::pair<std::string, std::string> > messages;
  Status status{Status::Kind::Complete, {}};
};

/// Run `model`, reading `count` decisions out of every solution.
Collected solve(fznso::Library& lib, const LayeredModel& model, std::size_t count,
                const std::vector<std::pair<std::string, OwnedValue> >& options = {},
                std::size_t stopAfter = 0) {
  fznso::DynSolver solver = lib.create_solver();
  for (const std::pair<std::string, OwnedValue>& o : options) {
    std::optional<std::string> error = solver.option_set(o.first, fznso::Value{o.second});
    check(!error.has_value(),
          error.has_value() ? error->c_str() : "option rejected");
  }
  Collected out;
  bool stop = false;
  out.status = solver.run(
    model,
    [&](const Solution& s) {
      std::vector<std::int64_t> values;
      for (std::size_t i = 0; i < count; i++) {
        fznso::Value v = s[Decision{i}];
        switch (v.kind()) {
        case FznsoValueBool: values.push_back(v.as_bool() ? 1 : 0); break;
        case FznsoValueInt: values.push_back(v.as_int()); break;
        case FznsoValueFloat: values.push_back(static_cast<std::int64_t>(v.as_float())); break;
        // A set is summarised by its cardinality, which is enough to tell the
        // assignments in these models apart.
        case FznsoValueSetInt: {
          std::int64_t n = 0;
          for (std::size_t r = 0; r < v.size(); r++) {
            fznso::Range<std::int64_t> range = v.int_range(r);
            n += range.max - range.min + 1;
          }
          values.push_back(n);
          break;
        }
        default: values.push_back(-9999); break;
        }
      }
      out.solutions.push_back(std::move(values));
      if (stopAfter != 0 && out.solutions.size() >= stopAfter) {
        stop = true;
      }
    },
    [&](std::string_view scope, fznso::Value value) {
      out.messages.emplace_back(std::string(scope),
                                value.kind() == FznsoValueString
                                  ? std::string(value.as_string()) : std::string());
    },
    [&]() { return stop; });
  return out;
}

OwnedValue list(std::vector<OwnedValue> items) { return OwnedValue{std::move(items)}; }

// --- the checks ------------------------------------------------------------

/// `x + y = 10`, `x <= y`, over `0..10`: two integer decisions and two
/// different constraint shapes.
void intModel(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(INT, OwnedValue::int_range(0, 10), "x");
  Decision y = m.add_decision(INT, OwnedValue::int_range(0, 10), "y");
  m.add_constraint("int_lin_eq",
                   {list({OwnedValue{std::int64_t{1}}, OwnedValue{std::int64_t{1}}}),
                    list({OwnedValue{x}, OwnedValue{y}}), OwnedValue{std::int64_t{10}}});
  m.add_constraint("int_le", {OwnedValue{x}, OwnedValue{y}});

  Collected one = solve(lib, m, 2);
  check(one.solutions.size() == 1, "int model reports one solution by default");
  if (!one.solutions.empty()) {
    std::int64_t a = one.solutions[0][0];
    std::int64_t b = one.solutions[0][1];
    check(a + b == 10 && a <= b, "int model solution satisfies the constraints");
  }

  Collected all = solve(lib, m, 2, {{"all_solutions", OwnedValue{true}}});
  check(all.solutions.size() == 6, "x+y=10, x<=y, 0..10 has six solutions");
  check(all.status.complete(), "an exhausted search is complete");
}

/// A Boolean model. The Boolean decisions have no domain at all — only their
/// declared type says they are `var bool` — and `bool2int` links one of them to
/// an integer variable.
void boolModel(fznso::Library& lib) {
  LayeredModel m;
  Decision a = m.add_decision(BOOL, OwnedValue{}, "a");
  Decision b = m.add_decision(BOOL, OwnedValue{}, "b");
  Decision n = m.add_decision(INT, OwnedValue::int_range(0, 1), "n");
  // a \/ b
  m.add_constraint("bool_clause", {list({OwnedValue{a}, OwnedValue{b}}), list({})});
  // not a
  m.add_constraint("bool_eq", {OwnedValue{a}, OwnedValue{false}});
  m.add_constraint("bool2int", {OwnedValue{b}, OwnedValue{n}});

  Collected out = solve(lib, m, 3, {{"all_solutions", OwnedValue{true}}});
  check(out.solutions.size() == 1, "the Boolean model has exactly one solution");
  if (!out.solutions.empty()) {
    check(out.solutions[0][0] == 0, "a is false");
    check(out.solutions[0][1] == 1, "b is true");
    check(out.solutions[0][2] == 1, "bool2int channels b onto n");
  }
}

/// A float model: `x + y = 3.5` with `x = 1.5`.
void floatModel(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(FLOAT, OwnedValue::float_range(0.0, 10.0), "x");
  Decision y = m.add_decision(FLOAT, OwnedValue::float_range(0.0, 10.0), "y");
  m.add_constraint("float_lin_eq",
                   {list({OwnedValue{1.0}, OwnedValue{1.0}}),
                    list({OwnedValue{x}, OwnedValue{y}}), OwnedValue{3.5}});
  m.add_constraint("float_eq", {OwnedValue{x}, OwnedValue{1.5}});

  fznso::DynSolver solver = lib.create_solver();
  double last = 0.0;
  bool seen = false;
  Status status = solver.run(m, [&](const Solution& s) {
    fznso::Value v = s[Decision{1}];
    check(v.kind() == FznsoValueFloat, "a float decision reports a float");
    if (v.kind() == FznsoValueFloat) {
      last = v.as_float();
      seen = true;
    }
  });
  check(seen, "the float model has a solution");
  check(!status.failed(), "the float model does not error");
  check(seen && last > 1.9 && last < 2.1, "y is 2.0");
}

/// A set model: `s` is a subset of `1..5` containing `3`, with `|s| = 2`.
void setModel(fznso::Library& lib) {
  LayeredModel m;
  Decision s = m.add_decision(SET, OwnedValue::int_range(1, 5), "s");
  Decision c = m.add_decision(INT, OwnedValue::int_range(2, 2), "c");
  m.add_constraint("set_in", {OwnedValue{std::int64_t{3}}, OwnedValue{s}});
  m.add_constraint("set_card", {OwnedValue{s}, OwnedValue{c}});

  Collected out = solve(lib, m, 1, {{"all_solutions", OwnedValue{true}}});
  check(out.solutions.size() == 4, "four two-element subsets of 1..5 contain 3");
  for (const std::vector<std::int64_t>& sol : out.solutions)
    check(sol[0] == 2, "every reported set has two elements");
}

/// Minimising `x` subject to `x >= 4`, with and without intermediate reporting.
void objectiveModel(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(INT, OwnedValue::int_range(0, 20), "x");
  m.add_constraint("int_le", {OwnedValue{std::int64_t{4}}, OwnedValue{x}});
  m.set_objective("minimize_int", OwnedValue{x});

  Collected best = solve(lib, m, 1);
  check(best.solutions.size() == 1, "without `intermediate` only the best is reported");
  check(!best.solutions.empty() && best.solutions[0][0] == 4, "the minimum is 4");
  check(best.status.complete(), "a finished optimisation is complete");

  // `createBranchers` branches the objective last and towards its best value,
  // so an objective that nothing else determines is optimal in the first
  // solution. Making it the sum of two searched variables is what produces
  // genuinely improving solutions.
  LayeredModel up;
  Decision a = up.add_decision(INT, OwnedValue::int_range(0, 2), "a");
  Decision b = up.add_decision(INT, OwnedValue::int_range(0, 3), "b");
  Decision z = up.add_decision(INT, OwnedValue::int_range(0, 5), "z");
  up.add_constraint("int_lin_eq",
                    {list({OwnedValue{std::int64_t{1}}, OwnedValue{std::int64_t{1}},
                           OwnedValue{std::int64_t{-1}}}),
                     list({OwnedValue{a}, OwnedValue{b}, OwnedValue{z}}),
                     OwnedValue{std::int64_t{0}}});
  up.set_objective("maximize_int", OwnedValue{z});

  Collected each = solve(lib, up, 3, {{"intermediate", OwnedValue{true}}});
  check(each.solutions.size() > 1, "with `intermediate` the improving solutions are reported");
  check(!each.solutions.empty() && each.solutions.back()[2] == 5,
        "the last intermediate solution is the optimum");
  bool bound = false;
  for (const std::pair<std::string, std::string>& msg : each.messages)
    if (msg.first == "progress.bound")
      bound = true;
  check(bound, "improving solutions report a bound");

  Collected quiet = solve(lib, up, 3);
  check(quiet.solutions.size() == 1, "without `intermediate` only the best is reported");
  check(!quiet.solutions.empty() && quiet.solutions[0][2] == 5, "the maximum is 5");
}

/// Every solution attaining the optimum, which needs the second search pass.
void allOptimaModel(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(INT, OwnedValue::int_range(0, 3), "x");
  Decision y = m.add_decision(INT, OwnedValue::int_range(0, 3), "y");
  Decision z = m.add_decision(INT, OwnedValue::int_range(0, 6), "z");
  m.add_constraint("int_lin_eq",
                   {list({OwnedValue{std::int64_t{1}}, OwnedValue{std::int64_t{1}},
                          OwnedValue{std::int64_t{-1}}}),
                    list({OwnedValue{x}, OwnedValue{y}, OwnedValue{z}}),
                    OwnedValue{std::int64_t{0}}});
  m.add_constraint("int_le", {OwnedValue{std::int64_t{2}}, OwnedValue{z}});
  m.set_objective("minimize_int", OwnedValue{z});

  Collected out = solve(lib, m, 3, {{"all_solutions", OwnedValue{true}}});
  check(out.solutions.size() == 3, "x+y=2 has three solutions over 0..3");
  for (const std::vector<std::int64_t>& sol : out.solutions)
    check(sol[2] == 2, "every reported solution attains the optimum");
}

/// An unsatisfiable model finishes complete, having reported nothing.
void unsatModel(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(INT, OwnedValue::int_range(0, 5), "x");
  m.add_constraint("int_le", {OwnedValue{std::int64_t{4}}, OwnedValue{x}});
  m.add_constraint("int_le", {OwnedValue{x}, OwnedValue{std::int64_t{2}}});

  Collected out = solve(lib, m, 1);
  check(out.solutions.empty(), "an unsatisfiable model reports no solutions");
  check(out.status.complete(), "an unsatisfiable model is complete");
}

/// Answering `should_stop` from inside `on_solution` ends the run.
void stopSignal(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(INT, OwnedValue::int_range(0, 100), "x");
  m.add_constraint("int_le", {OwnedValue{std::int64_t{0}}, OwnedValue{x}});

  Collected out = solve(lib, m, 1, {{"all_solutions", OwnedValue{true}}}, 3);
  check(out.solutions.size() == 3, "the run stops after the third solution");
  check(out.status.kind == Status::Kind::Incomplete, "a cancelled run is incomplete");
}

/// A second layer constrains the model further, and the first layer's
/// constraints still hold. The clone of layer 0 is reused across both runs.
void layers(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(INT, OwnedValue::int_range(0, 10), "x");
  m.add_constraint("int_le", {OwnedValue{std::int64_t{3}}, OwnedValue{x}});

  fznso::DynSolver solver = lib.create_solver();
  std::optional<std::string> error = solver.option_set("all_solutions", fznso::Value{true});
  check(!error.has_value(), "all_solutions accepted");
  // `verbose` logs a line per layer actually posted, which is how the checks
  // below tell a reused clone from a rebuilt one.
  check(!solver.option_set("verbose", fznso::Value{true}).has_value(), "verbose accepted");

  // Counts the "posted layer" lines of one run.
  int posted = 0;
  auto countPosted = [&](std::string_view scope, fznso::Value value) {
    if (scope == "log" && value.kind() == FznsoValueString &&
        value.as_string().find("posted layer") != std::string_view::npos)
      posted++;
  };

  std::vector<std::int64_t> first;
  Status s1 = solver.run(m, [&](const Solution& s) { first.push_back(s[x].as_int()); },
                         countPosted);
  check(s1.complete() && first.size() == 8, "layer 0 alone has eight solutions");
  check(posted == 1, "the first run posts layer 0");

  m.push_layer();
  Decision y = m.add_decision(INT, OwnedValue::int_range(0, 10), "y");
  m.add_constraint("int_le", {OwnedValue{x}, OwnedValue{std::int64_t{5}}});
  m.add_constraint("int_lin_eq",
                   {list({OwnedValue{std::int64_t{1}}, OwnedValue{std::int64_t{-1}}}),
                    list({OwnedValue{x}, OwnedValue{y}}), OwnedValue{std::int64_t{0}}});
  // Layer 0 is untouched, so the solver may keep the space it built for it.
  m.set_unchanged(1);

  posted = 0;
  std::vector<std::pair<std::int64_t, std::int64_t> > second;
  Status s2 = solver.run(m, [&](const Solution& s) {
    second.emplace_back(s[x].as_int(), s[y].as_int());
  }, countPosted);
  check(s2.complete() && second.size() == 3, "with layer 1 only 3..5 remain");
  check(posted == 1, "the second run posts only the new layer");
  for (const std::pair<std::int64_t, std::int64_t>& sol : second) {
    check(sol.first >= 3 && sol.first <= 5, "layer 0's constraint still holds");
    check(sol.first == sol.second, "layer 1's constraint holds");
  }

  // Retracting the layer restores the original answers.
  m.pop_layer();
  m.set_unchanged(1);
  posted = 0;
  std::vector<std::int64_t> third;
  Status s3 = solver.run(m, [&](const Solution& s) { third.push_back(s[Decision{0}].as_int()); },
                         countPosted);
  check(s3.complete() && third.size() == 8, "popping layer 1 restores eight solutions");
  check(posted == 0, "popping a layer posts nothing again");
}

/// An identifier this solver does not accept is an error, not a crash.
void unknownConstraint(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(INT, OwnedValue::int_range(0, 3), "x");
  m.add_constraint("no_such_constraint", {OwnedValue{x}});

  fznso::DynSolver solver = lib.create_solver();
  Status status = solver.run(m, [](const Solution&) {});
  check(status.failed(), "an unknown constraint fails the run");
  check(status.error.find("no_such_constraint") != std::string::npos,
        "the error names the constraint");
}

/// A decision used where its declared type does not fit is rejected.
void conflictingTypes(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(BOOL, OwnedValue{}, "x");
  m.add_constraint("bool_eq", {OwnedValue{x}, OwnedValue{true}});
  // `x` is a Boolean, so it cannot stand in the integer position of `int_le`.
  m.add_constraint("int_le", {OwnedValue{x}, OwnedValue{std::int64_t{1}}});

  fznso::DynSolver solver = lib.create_solver();
  Status status = solver.run(m, [](const Solution&) {});
  check(status.failed(), "a Boolean used in an integer position fails the run");
}

/// A variable's type comes from the model, not from its domain: these two have
/// exactly the same (absent) domain and differ only in what they are declared
/// to be.
void typeDrivesTheVariable(fznso::Library& lib) {
  LayeredModel m;
  Decision b = m.add_decision(BOOL, OwnedValue{}, "b");
  Decision i = m.add_decision(INT, OwnedValue::int_range(0, 3), "i");
  m.add_constraint("bool_eq", {OwnedValue{b}, OwnedValue{true}});
  m.add_constraint("int_le", {OwnedValue{std::int64_t{2}}, OwnedValue{i}});

  fznso::DynSolver solver = lib.create_solver();
  bool checked = false;
  Status status = solver.run(m, [&](const Solution& s) {
    check(s[b].kind() == FznsoValueBool, "a declared Boolean reports a Boolean");
    check(s[i].kind() == FznsoValueInt, "a declared integer reports an integer");
    check(s[b].kind() == FznsoValueBool && s[b].as_bool(), "b is true");
    checked = true;
  });
  check(!status.failed() && checked, "the mixed-type model solves");
}

/// A constraint Gecode registers as `gecode_circuit` is declared, and accepted,
/// as plain `circuit` — and the prefixed spellings still resolve to it.
void libraryPrefixes(fznso::Library& lib) {
  const char* names[] = {"circuit", "gecode_circuit", "fzn_circuit"};
  for (const char* name : names) {
    LayeredModel m;
    std::vector<OwnedValue> succ;
    for (int i = 0; i < 3; i++) {
      succ.push_back(OwnedValue{m.add_decision(INT, OwnedValue::int_range(1, 3),
                                               "s" + std::to_string(i))});
    }
    m.add_constraint(name, {OwnedValue{std::int64_t{1}}, list(succ)});

    Collected out = solve(lib, m, 3, {{"all_solutions", OwnedValue{true}}});
    check(out.solutions.size() == 2,
          "a three-node circuit has two solutions whatever it is called");
    check(!out.status.failed(), "the circuit model does not error");
  }

  // The same, for a constraint the registry knows under its own name: the
  // `fzn_` spelling a MiniZinc library emits has to resolve too.
  for (const char* name : {"all_different_int", "fzn_all_different_int"}) {
    LayeredModel m;
    std::vector<OwnedValue> xs;
    for (int i = 0; i < 3; i++) {
      xs.push_back(OwnedValue{m.add_decision(INT, OwnedValue::int_range(1, 3),
                                             "x" + std::to_string(i))});
    }
    m.add_constraint(name, {list(xs)});

    Collected out = solve(lib, m, 3, {{"all_solutions", OwnedValue{true}}});
    check(out.solutions.size() == 6, "three distinct values over 1..3 give six solutions");
  }
}

/// Everything declared has to be postable, or a consumer would be lied to.
///
/// A declared name is not the name it posts under: the registry knows some
/// constraints as `gecode_<name>`, and others under a name Gecode chose rather
/// than the one a consumer's own interface uses. What has to hold is that the
/// declared name resolves to a signature, and that the signature's posting name
/// is one the registry knows.
void declaredConstraintsExist(fznso::Library& lib) {
  std::vector<std::string> known = Gecode::FlatZinc::registry().identifiers();
  std::set<std::string> registered(known.begin(), known.end());
  FznsoConstraintList list = lib.constraint_types();
  check(list.len > 200, "the constraint list is not obviously truncated");
  // The restart-based constraints are collected while the model is translated
  // and turned into the arrays the search reads, so they are never posted.
  std::set<std::string> declaredTwice;
  for (std::size_t i = 0; i < list.len; i++) {
    const FznsoConstraintType& c = list.constraints[i];
    std::string ident(c.ident.ptr, c.ident.len);
    const Gecode::FlatZinc::Signature* sig = Gecode::FlatZinc::signature(ident);
    if (sig == nullptr) {
      std::fprintf(stderr, "FAIL: declared constraint `%s` does not resolve\n",
                   ident.c_str());
      failures++;
      continue;
    }
    if (ident.rfind("on_restart_", 0) != 0 &&
        registered.count(std::string(sig->posts_as)) == 0) {
      std::fprintf(stderr, "FAIL: declared constraint `%s` posts as `%s`, "
                   "which is not in the registry\n",
                   ident.c_str(), std::string(sig->posts_as).c_str());
      failures++;
    }
    if (sig->permutation != nullptr) {
      std::set<std::size_t> seen(sig->permutation->begin(), sig->permutation->end());
      check(sig->permutation->size() == c.arg_len && seen.size() == c.arg_len &&
            (seen.empty() || *seen.rbegin() == c.arg_len - 1),
            "an argument permutation covers every declared argument once");
    }
    // A name may be declared more than once, for the argument types a solver
    // accepts it with. A model identifier resolves to only one of them, so the
    // forms have to agree on everything that resolution is used for: where the
    // constraint posts, and what each argument's base type is.
    for (std::size_t j = 0; j < i; j++) {
      const FznsoConstraintType& d = list.constraints[j];
      if (d.arg_len != c.arg_len || std::string(d.ident.ptr, d.ident.len) != ident) {
        continue;
      }
      bool agrees = true;
      for (std::size_t k = 0; k < c.arg_len; k++) {
        agrees = agrees && d.arg_types[k].base == c.arg_types[k].base &&
                 d.arg_types[k].set_of == c.arg_types[k].set_of &&
                 d.arg_types[k].list_of == c.arg_types[k].list_of;
      }
      if (!agrees) {
        declaredTwice.insert(ident);
      }
    }
  }
  for (const std::string& ident : declaredTwice) {
    std::fprintf(stderr,
                 "FAIL: the forms of `%s` disagree on an argument's base type\n",
                 ident.c_str());
    failures++;
  }
}

/// Statistics survive a run and report something plausible.
void statistics(fznso::Library& lib) {
  LayeredModel m;
  Decision x = m.add_decision(INT, OwnedValue::int_range(0, 20), "x");
  m.add_constraint("int_le", {OwnedValue{std::int64_t{4}}, OwnedValue{x}});
  m.set_objective("minimize_int", OwnedValue{x});

  fznso::DynSolver solver = lib.create_solver();
  std::int64_t reported = -1;
  Status status = solver.run(m, [&](const Solution& s) {
    fznso::Value v = s.statistic("solutions");
    if (v.kind() == FznsoValueInt)
      reported = v.as_int();
  });
  check(!status.failed(), "the statistics model does not error");
  check(reported >= 1, "a solution reports its own count");

  fznso::Value nodes = solver.statistic("nodes");
  fznso::Value vars = solver.statistic("variables");
  check(nodes.kind() == FznsoValueInt && nodes.as_int() > 0, "the run explored some nodes");
  check(vars.kind() == FznsoValueInt && vars.as_int() == 1, "the model has one decision");
  // Both values must still be readable: a statistic is borrowed until the
  // solver changes, not until the next read.
  check(nodes.kind() == FznsoValueInt && nodes.as_int() > 0,
        "an earlier statistic survives a later one");
  check(solver.statistic("no_such_statistic").kind() == FznsoValueAbsent,
        "an unknown statistic is absent");
}

/// A rejected option says why, and does not change anything.
void options(fznso::Library& lib) {
  fznso::DynSolver solver = lib.create_solver();
  check(solver.option_set("threads", fznso::Value{std::int64_t{2}}) == std::nullopt,
        "threads accepts an integer");
  check(solver.option_get("threads").as_int() == 2, "threads reads back");
  check(solver.option_set("restart", fznso::Value{std::string{"luby"}}) == std::nullopt,
        "restart accepts a known sequence");
  std::optional<std::string> bad =
    solver.option_set("restart", fznso::Value{std::string{"spiral"}});
  check(bad.has_value() && bad->find("spiral") != std::string::npos,
        "an unknown restart sequence is rejected by name");
  check(solver.option_get("restart").as_string() == "luby",
        "a rejected option leaves the old value in place");
  check(solver.option_set("nonesuch", fznso::Value{true}).has_value(),
        "an unknown option is rejected");
  check(solver.option_get("time_limit").kind() == FznsoValueAbsent,
        "time_limit defaults to no limit");
}

}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s <path to the FZnSO gecode library>\n", argv[0]);
    return 2;
  }
  fznso::Library lib{argv[1]};

  intModel(lib);
  boolModel(lib);
  floatModel(lib);
  setModel(lib);
  objectiveModel(lib);
  allOptimaModel(lib);
  unsatModel(lib);
  stopSignal(lib);
  layers(lib);
  unknownConstraint(lib);
  conflictingTypes(lib);
  typeDrivesTheVariable(lib);
  libraryPrefixes(lib);
  declaredConstraintsExist(lib);
  statistics(lib);
  options(lib);

  if (failures != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }
  std::printf("all checks passed\n");
  return 0;
}
