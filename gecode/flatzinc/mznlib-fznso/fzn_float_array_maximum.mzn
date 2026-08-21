% Gecode has a binary float maximum propagator but nothing for the array form,
% and the shared library aborts rather than guess a decomposition. A chain of the
% binary one is what that costs, and it keeps a real propagator on every step
% rather than lowering the whole thing to linear constraints.
%
% The propagator is reached under `gecode_float_max` rather than under the
% FlatZinc name `float_max`: `redefinitions.mzn` rewrites that builtin onto
% `fzn_float_array_maximum`, which is this predicate, so naming it here would
% close a loop and the flattener would not terminate.

predicate gecode_float_max(var float: a, var float: b, var float: c);

predicate fzn_float_array_maximum(list of var float: xs, var float: value) =
  let {
    int: n = length(xs);
    array [1..n] of var float: running;
  } in running[1] = xs[1] /\
    value = running[n] /\
    forall (i in 2..n) (gecode_float_max(running[i - 1], xs[i], running[i]));
