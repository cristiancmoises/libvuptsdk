# Copyright 2026 Cristian Cezar Moisés
# SPDX-License-Identifier: Apache-2.0
# Identical balanced input for every native thread-count measurement.
# Includes tree construction and reduction; does not execute the C SDK.
import Base
import ./aggregation.bend as A

def main() -> IO(Unit):
  IO.print(A.show(A.aggregate(A.generate(20n, 0))))
