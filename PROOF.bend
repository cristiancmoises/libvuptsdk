# Copyright 2026 Cristian Cezar Moisés
# SPDX-License-Identifier: Apache-2.0
import Base
import ./formal/aggregation.bend as A
import ./LAWS.bend as L

def L.add_zero_right(x):
  match x:
    case 0n:
      {==}
    case 1n+p:
      %L.add_zero_right(p) : {1n+Nat.add(p, 0n) == 1n+_ : Nat}
      {==}

def L.add_associative(x, y, z):
  match x:
    case 0n:
      {==}
    case 1n+p:
      %L.add_associative(p, y, z) : {1n+Nat.add(Nat.add(p, y), z) == 1n+_ : Nat}
      {==}

def L.merge_zero_left(summary):
  match summary:
    case A.Totals{count, bytes}:
      {==}

def L.merge_zero_right(summary):
  match summary:
    case A.Totals{count, bytes}:
      %L.add_zero_right(count) : {A.Totals{Nat.add(count, 0n), Nat.add(bytes, 0n)} == A.Totals{_, bytes} : A.Summary}
      %L.add_zero_right(bytes) : {A.Totals{Nat.add(count, 0n), Nat.add(bytes, 0n)} == A.Totals{Nat.add(count, 0n), _} : A.Summary}
      {==}

def L.merge_associative(a, b, c):
  match a b c:
    case A.Totals{ac, ab} A.Totals{bc, bb} A.Totals{cc, cb}:
      %L.add_associative(ac, bc, cc) : {A.Totals{Nat.add(Nat.add(ac, bc), cc), Nat.add(Nat.add(ab, bb), cb)} == A.Totals{_, Nat.add(ab, Nat.add(bb, cb))} : A.Summary}
      %L.add_associative(ab, bb, cb) : {A.Totals{Nat.add(Nat.add(ac, bc), cc), Nat.add(Nat.add(ab, bb), cb)} == A.Totals{Nat.add(Nat.add(ac, bc), cc), _} : A.Summary}
      {==}

def L.append_associative(a, b, c):
  match a:
    case A.End{}:
      {==}
    case A.More{bytes, rest}:
      %L.append_associative(rest, b, c) : {A.More{bytes, A.append(A.append(rest, b), c)} == A.More{bytes, _} : A.Blocks}
      {==}

def L.sequential_append(a, b):
  match a:
    case A.End{}:
      Equal.sym(A.Summary, A.merge(A.Totals{0n, 0n}, A.sequential(b)), A.sequential(b), L.merge_zero_left(A.sequential(b)))
    case A.More{+bytes, +rest}:
      +b = b
      %Equal.sym(A.Summary, A.sequential(A.append(rest, b)), A.merge(A.sequential(rest), A.sequential(b)), L.sequential_append(rest, b)) : {A.merge(A.Totals{1n, bytes}, _) == A.merge(A.merge(A.Totals{1n, bytes}, A.sequential(rest)), A.sequential(b)) : A.Summary}
      Equal.sym(A.Summary, A.merge(A.merge(A.Totals{1n, bytes}, A.sequential(rest)), A.sequential(b)), A.merge(A.Totals{1n, bytes}, A.merge(A.sequential(rest), A.sequential(b))), L.merge_associative(A.Totals{1n, bytes}, A.sequential(rest), A.sequential(b)))

def L.aggregate_flatten(tree):
  match tree:
    case A.Empty{}:
      {==}
    case A.Leaf{bytes}:
      Equal.sym(A.Summary, A.merge(A.Totals{1n, bytes}, A.Totals{0n, 0n}), A.Totals{1n, bytes}, L.merge_zero_right(A.Totals{1n, bytes}))
    case A.Fork{+left, +right}:
      %Equal.sym(A.Summary, A.aggregate(left), A.sequential(A.flatten(left)), L.aggregate_flatten(left)) : {A.merge(_, A.aggregate(right)) == A.sequential(A.append(A.flatten(left), A.flatten(right))) : A.Summary}
      %Equal.sym(A.Summary, A.aggregate(right), A.sequential(A.flatten(right)), L.aggregate_flatten(right)) : {A.merge(A.sequential(A.flatten(left)), _) == A.sequential(A.append(A.flatten(left), A.flatten(right))) : A.Summary}
      Equal.sym(A.Summary, A.sequential(A.append(A.flatten(left), A.flatten(right))), A.merge(A.sequential(A.flatten(left)), A.sequential(A.flatten(right))), L.sequential_append(A.flatten(left), A.flatten(right)))

def L.regroup_order(a, b, c):
  L.append_associative(A.flatten(a), A.flatten(b), A.flatten(c))

def L.regroup_summary(a, b, c):
  L.merge_associative(A.aggregate(a), A.aggregate(b), A.aggregate(c))
