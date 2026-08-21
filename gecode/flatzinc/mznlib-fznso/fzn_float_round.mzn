% See `fzn_float_ceil.mzn`. Rounding away from zero, so the sign decides which
% way the half goes.

include "fzn_float_ceil.mzn";
include "fzn_float_floor.mzn";

predicate fzn_float_round(var float: a, var int: b) =
  if lb(a) >= 0.0 then
    fzn_float_floor(a + 0.5, b)
  elseif ub(a) <= 0.0 then
    fzn_float_ceil(a - 0.5, b)
  else
    if a < 0.0 then fzn_float_ceil(a - 0.5, b) else fzn_float_floor(a + 0.5, b) endif
  endif;
