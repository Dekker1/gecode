% Gecode has no rounding propagator, and the shared library aborts rather than
% guess a decomposition. Here it is a pair of bounds on the float, which is what
% Gecode's own float relations express directly.

predicate fzn_float_ceil(var float: a, var int: b) = b - 1 < a /\ a <= b;
