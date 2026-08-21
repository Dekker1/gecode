% Gecode has a two-dimensional element propagator, and this is where the case
% it covers is picked out.
%
% The registry's identifier is k-dimensional and a solver cannot narrow arity —
% only types — so declaring it would promise every number of dimensions. Instead
% the identifier is left undeclared and overridden here: two dimensions reach
% the propagator, anything else falls back to a flat index and the
% one-dimensional element, which Gecode does declare.
%
% `gecode_int_element2d` takes an index set per dimension rather than an offset
% and a size, so the ranges are rebuilt from the pair.

include "fzn_int_array_element.mzn";

predicate gecode_int_element2d(
  var int: idx1,
  var int: idx2,
  array [int] of var int: x,
  set of int: idxset1,
  set of int: idxset2,
  var int: c,
);

predicate fzn_int_array_element_nd(
  list of var int: xs,
  list of int: offsets,
  list of int: sizes,
  list of var int: indices,
  var int: value,
) =
  if length(sizes) = 2 then
    gecode_int_element2d(
      indices[1],
      indices[2],
      xs,
      offsets[1]..offsets[1] + sizes[1] - 1,
      offsets[2]..offsets[2] + sizes[2] - 1,
      value,
    )
  else
    let {
      int: d = length(sizes);
      array [1..d] of int: stride = [product (e in i + 1..d) (sizes[e]) | i in 1..d];
      var int: flat = 1 + sum (i in 1..d) (stride[i] * (indices[i] - offsets[i]));
    } in fzn_int_array_element(xs, 1, flat, value)
  endif;
