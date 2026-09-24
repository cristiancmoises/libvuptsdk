# Copyright 2026 Cristian Cezar Moisés
# SPDX-License-Identifier: Apache-2.0
# Pure archive block metadata model. Naturals exclude machine-word overflow.
import Base

type Blocks is Data:
  End{}
  More{bytes: Nat, rest: Blocks}

type Tree is Data:
  Empty{}
  Leaf{bytes: Nat}
  Fork{left: Tree, right: Tree}

type Summary is Data:
  Totals{count: Nat, bytes: Nat}

def merge(left: Summary, right: Summary) -> Summary:
  match left right:
    case Totals{lc, lb} Totals{rc, rb}:
      Totals{Nat.add(lc, rc), Nat.add(lb, rb)}

def append(left: Blocks, +right: Blocks) -> Blocks:
  match left:
    case End{}:
      right
    case More{bytes, rest}:
      More{bytes, append(rest, right)}

def flatten(tree: Tree) -> Blocks:
  match tree:
    case Empty{}:
      End{}
    case Leaf{bytes}:
      More{bytes, End{}}
    case Fork{left, right}:
      append(flatten(left), flatten(right))

def sequential(blocks: Blocks) -> Summary:
  match blocks:
    case End{}:
      Totals{0n, 0n}
    case More{bytes, rest}:
      merge(Totals{1n, bytes}, sequential(rest))

def aggregate(tree: Tree) -> Summary:
  match tree:
    case Empty{}:
      Totals{0n, 0n}
    case Leaf{bytes}:
      Totals{1n, bytes}
    case Fork{left, right}:
      left_total right_total = aggregate(left) aggregate(right)
      merge(left_total, right_total)

# Leaves enumerate [start, start + 2^depth) in archive order. Sizes vary from
# 1 to 256 bytes; the shape is balanced so sibling reductions have equal work.
def generate(+depth: Nat, +start: U32) -> Tree:
  match depth:
    case 0n:
      Leaf{U32.to_nat((start % 256 + 1 : U32))}
    case 1n+previous:
      left right = generate(previous, start) generate(previous, (start + (1 << previous : U32) : U32))
      Fork{left, right}

def show(summary: Summary) -> String:
  match summary:
    case Totals{count, bytes}:
      Nat.show(count) ++ " " ++ Nat.show(bytes)

def main() -> IO(Unit):
  IO.print(show(aggregate(generate(4n, 0))))
