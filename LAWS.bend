# Copyright 2026 Cristian Cezar Moisés
# SPDX-License-Identifier: Apache-2.0
# Immutable contract for the Bend model, not a proof of the C implementation.
import Base
import ./formal/aggregation.bend as A

law add_zero_right:
  for x: Nat
  {Nat.add(x, 0n) == x : Nat}

law add_associative:
  for x: Nat
  for y: Nat
  for z: Nat
  {Nat.add(Nat.add(x, y), z) == Nat.add(x, Nat.add(y, z)) : Nat}

law merge_zero_left:
  for summary: A.Summary
  {A.merge(A.Totals{0n, 0n}, summary) == summary : A.Summary}

law merge_zero_right:
  for summary: A.Summary
  {A.merge(summary, A.Totals{0n, 0n}) == summary : A.Summary}

law merge_associative:
  for a: A.Summary
  for b: A.Summary
  for c: A.Summary
  {A.merge(A.merge(a, b), c) == A.merge(a, A.merge(b, c)) : A.Summary}

law append_associative:
  for a: A.Blocks
  for b: A.Blocks
  for c: A.Blocks
  {A.append(A.append(a, b), c) == A.append(a, A.append(b, c)) : A.Blocks}

law sequential_append:
  for a: A.Blocks
  for b: A.Blocks
  {A.sequential(A.append(a, b)) == A.merge(A.sequential(a), A.sequential(b)) : A.Summary}

# Every leaf contributes once: both count and bytes equal the independent
# sequential fold of the leaves in archive order, for every finite tree.
law aggregate_flatten:
  for tree: A.Tree
  {A.aggregate(tree) == A.sequential(A.flatten(tree)) : A.Summary}

# Rotation preserves the exact ordered leaf sequence, not merely its sum.
law regroup_order:
  for a: A.Tree
  for b: A.Tree
  for c: A.Tree
  {A.flatten(A.Fork{A.Fork{a, b}, c}) == A.flatten(A.Fork{a, A.Fork{b, c}}) : A.Blocks}

law regroup_summary:
  for a: A.Tree
  for b: A.Tree
  for c: A.Tree
  {A.aggregate(A.Fork{A.Fork{a, b}, c}) == A.aggregate(A.Fork{a, A.Fork{b, c}}) : A.Summary}
