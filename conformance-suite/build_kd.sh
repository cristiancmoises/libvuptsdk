#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Build the `kd` helper used by the differential runners (real system RNG).
set -eu
suite=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
output=${1:-./kd}
sh "$suite/build_driver.sh" "$suite/kd_helper.c" "$output"
echo "built $output"
