%
%  Main authors:
%     Guido Tack <tack@gecode.dev>
%
%  Copyright:
%     Guido Tack, 2007
%
%  This file is part of Gecode, the generic constraint
%  development environment:
%     http://www.gecode.dev
%
%  Permission is hereby granted, free of charge, to any person obtaining
%  a copy of this software and associated documentation files (the
%  "Software"), to deal in the Software without restriction, including
%  without limitation the rights to use, copy, modify, merge, publish,
%  distribute, sublicense, and/or sell copies of the Software, and to
%  permit persons to whom the Software is furnished to do so, subject to
%  the following conditions:
%
%  The above copyright notice and this permission notice shall be
%  included in all copies or substantial portions of the Software.
%
%  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
%  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
%  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
%  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
%  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
%  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
%  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
%
%

predicate float_sinh(var float: a, var float: b) =
    b == (exp(a)-exp(-a))/2.0;

predicate float_cosh(var float: a, var float: b) =
    b == (exp(a)+exp(-a))/2.0;

predicate float_tanh(var float: a, var float: b) =
    let { var float: e2a = exp(2.0*a) } in
    b == (e2a-1.0)/(e2a+1.0);

predicate float_ne_reif(var float: a, var float: b, var bool: c) =
    not c <-> a==b;

predicate array_float_element(var int: b, array[int] of float: as, var float: c) =
    b in index_set(as) /\
    forall(d in index_set(as))( b = d -> as[d] = c );

predicate array_var_float_element(var int: b, array[int] of var float: as, var float: c) =
    b in index_set(as) /\
    forall(d in index_set(as))( b = d -> as[d] = c );

predicate int_pow(var int: x, var int: y, var int: z) =
    let {
        array[lb(y)..ub(y)] of var int: pow_table = [
            i: if i in dom(y) then pow(x, i) default lb(z) else lb(z) endif
            | i in lb(y)..ub(y)
        ];
    } in z = pow_table[y];
