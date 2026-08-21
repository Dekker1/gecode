% Gecode has no float element propagator, and the shared library aborts rather
% than guess a decomposition. Here it is one implication per position, in the
% caller's own numbering.

predicate fzn_float_array_element(
  list of var float: xs,
  int: offset,
  var int: index,
  var float: value,
) =
  index in offset..offset + length(xs) - 1 /\
    forall (i in 1..length(xs)) (index = offset + i - 1 -> value = xs[i]);
