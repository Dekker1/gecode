% Gecode has `float_lin_eq` and its reified form but no `float_lin_ne`, and the
% shared library aborts rather than guess a decomposition. A disequality is the
% equality reified to false, which keeps Gecode's own propagator on it.

predicate fzn_float_lin_ne(list of float: coeffs, list of var float: xs, float: total) =
  float_lin_eq_reif(coeffs, xs, total, false);
