#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Cristian Cezar Moisés
# Build a standalone ML-KEM driver with the production primitive dependencies.
# Each driver supplies its own RNG, including deterministic ACVP seed injection.
set -eu
if [ "$#" -lt 2 ]; then
    echo "usage: $0 DRIVER.c OUTPUT [compiler flags...]" >&2
    exit 2
fi
driver=$1
output=$2
shift 2
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
"${CC:-cc}" -std=c11 -O2 -I"$root/include" -I"$root/src" "$@" \
    "$driver" "$root/src/zupt_mlkem.c" "$root/src/zupt_keccak.c" \
    "$root/src/zupt_ct.c" -o "$output" -lm
