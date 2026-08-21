% See `fzn_float_lin_ne.mzn`. Reified, the disequality is the equality against
% the opposite Boolean.

predicate fzn_float_lin_ne_reif(
  list of float: coeffs,
  list of var float: xs,
  float: total,
  var bool: b,
) = float_lin_eq_reif(coeffs, xs, total, not b);
