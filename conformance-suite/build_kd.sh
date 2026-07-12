#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# Build the `kd` helper used by the differential runners (real system RNG).
set -e
gcc -O2 -Iinclude -Isrc "$(dirname "$0")/kd_helper.c" \
    src/zupt_mlkem.c src/zupt_keccak.c src/zupt_sha256.c -o kd
echo "built ./kd"
