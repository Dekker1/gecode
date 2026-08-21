% Gecode's set element propagator is one-based and takes no offset, so this is
% where the offset the registry carries is dealt with.
%
% The propagator is reached under `gecode_set_element` rather than under the
% FlatZinc name `array_var_set_element`: `redefinitions.mzn` rewrites that
% builtin onto `fzn_set_of_int_array_element`, which is this predicate, so
% naming it here would close a loop and the flattener would not terminate.

predicate gecode_set_element(
  var int: idx,
  array [int] of var set of int: xs,
  var set of int: c,
);

predicate fzn_set_of_int_array_element(
  list of var set of int: xs,
  int: offset,
  var int: index,
  var set of int: value,
) =
  if offset = 1 then
    gecode_set_element(index, xs, value)
  else
    gecode_set_element(index - offset + 1, xs, value)
  endif;
