% See `fzn_float_ceil.mzn`.

predicate fzn_float_floor(var float: a, var int: b) = b <= a /\ a < b + 1;
