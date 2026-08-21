% Gecode has no `float_in` propagator, and the shared library aborts rather than
% guess a decomposition. Here it is a pair of bounds, which is what Gecode's own
% float relations express directly.

predicate fzn_float_in(var float: x, float: lower_bound, float: upper_bound) =
  x >= lower_bound /\ x <= upper_bound;
