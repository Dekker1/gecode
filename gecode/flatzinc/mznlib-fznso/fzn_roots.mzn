% Gecode has no propagator for `roots`. What it has is a channelling between an
% array of integers and an array of sets, and a union over the elements a set
% selects; together those say the same thing, so the constraint is decomposed
% into them here rather than left to the standard library's quadratic form.
%
% The rewrite needs a set variable per value `image` may hold, so every value
% `xs` can take has to be one `image` could hold. Only the model knows those
% bounds, which is why the guard lives here with the construction it guards.

include "int_set_channel.mzn";

predicate gecode_array_set_element_union(
  var set of int: selector,
  array [int] of var set of int: sets,
  var set of int: result,
);

predicate fzn_roots(array [int] of var int: xs, var set of int: preimage, var set of int: image) =
  if length(xs) = 0 \/ ub(preimage) = {} \/ ub(image) = {} \/ min(index_set(xs)) < 0 \/
    min(ub(image)) < 1 \/ lb_array(xs) < 0 \/ ub_array(xs) > max(ub(image))
  then
    forall (i in ub(preimage)) (i in preimage -> xs[i] in image) /\
      forall (i in ub(image)) (
        i in image -> forall (j in index_set(xs)) (xs[j] = i -> j in preimage)
      )
  else
    let {
      array [1..max(ub(image))] of var set of 0..max(index_set(xs)): y;
    } in int_set_channel(xs, y) /\ gecode_array_set_element_union(image, y, preimage)
  endif;
