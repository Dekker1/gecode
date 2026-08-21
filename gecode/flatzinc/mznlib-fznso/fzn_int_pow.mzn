% Gecode's propagator takes a constant, non-negative exponent, and this file is
% where the cases it cannot take are peeled off.
%
% A solver's own library sits outside the directories `enable_native_predicates`
% rewrites, so the fixed-argument dispatch the compiler generates for the overlay
% is not generated here: the narrow form has to be named explicitly, or the
% propagator is never reached.
%
% The variable-exponent case builds a table across the exponent's domain, so the
% guard is not redundant — for an unbounded `var int` that does not terminate.

predicate gecode_int_pow(var int: x, int: y, var int: z);

predicate fzn_int_pow(var int: a, var int: b, var int: c) =
  if is_fixed(b) then
    let { int: y = fix(b) } in if y = 0 then
      c = 1
    elseif y = 1 then
      c = a
    elseif y > 0 then
      gecode_int_pow(a, y, c)
    else
      c = 1 div pow(a, -y)
    endif
  else
    let {
      array [lb(b)..ub(b)] of var int: pow_table = [
        i: if i in dom(b) then pow(a, i) default lb(c) else lb(c) endif |
          i in lb(b)..ub(b),
      ];
    } in c = pow_table[b]
  endif;
