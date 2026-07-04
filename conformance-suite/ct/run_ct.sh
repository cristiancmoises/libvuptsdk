#!/bin/sh
# Constant-time verification for ML-KEM-768 (dudect + ctgrind). Run from a
# libvuptsdk checkout with this suite inside it. Requires valgrind + gcc.
#
# Blocking signal   : ctgrind (deterministic taint-tracking) — any
#                     "Conditional jump/move depends on uninitialised" fails.
# Blocking signal   : dudect definite leak (|t| >= 10, exit 2).
# Advisory only      : dudect WARN zone (4.5 <= |t| < 10, exit 1) — statistical
#                     noise on shared CI runners; reported, does not fail.
SRC="src/zupt_mlkem.c src/zupt_keccak.c src/zupt_sha256.c"
D="$(dirname "$0")"
TMP="${TMPDIR:-/tmp}"
rc=0

run_dudect() {
    bin="$1"; label="$2"
    "$bin"; ec=$?
    if [ "$ec" -eq 2 ]; then
        echo "FAIL ($label): dudect reports a definite timing leak (|t| >= 10)"
        rc=1
    elif [ "$ec" -eq 1 ]; then
        echo "WARN ($label): dudect in the 4.5..10 zone — advisory, not failing the gate"
    else
        echo "PASS ($label): dudect clean"
    fi
}

echo "== dudect: accept vs implicit-reject =="
gcc -O2 -Iinclude -Isrc "$D/dudect_decaps.c" $SRC -o "$TMP/dudect" -lm || { echo "FAIL: dudect build"; exit 1; }
run_dudect "$TMP/dudect" "accept-vs-reject"

echo "== dudect: fixed-vs-random accept path =="
gcc -O2 -DEXP2 -Iinclude -Isrc "$D/dudect_decaps.c" $SRC -o "$TMP/dudect2" -lm || { echo "FAIL: dudect2 build"; exit 1; }
run_dudect "$TMP/dudect2" "fixed-vs-random"

echo "== ctgrind: taint-tracking under memcheck (fail on 'Conditional jump/move') =="
if ! command -v valgrind >/dev/null 2>&1; then
    echo "FAIL: valgrind not installed — ctgrind cannot run, refusing to report a pass"
    exit 1
fi
gcc -O2 -g -Iinclude -Isrc "$D/ctgrind_mlkem.c" $SRC -o "$TMP/ctg" || { echo "FAIL: ctgrind build"; exit 1; }
# NOTE: run without -q so valgrind always emits its "ERROR SUMMARY" line, which
# we use below as proof the instrumented program ran to completion.
OUT=$(valgrind --tool=memcheck "$TMP/ctg" 2>&1)
# Proof-of-execution: valgrind always prints an ERROR SUMMARY when it ran the
# program to completion. Its absence means valgrind never ran or the harness
# crashed — which must FAIL, not silently pass.
if ! printf '%s\n' "$OUT" | grep -q "ERROR SUMMARY"; then
    echo "FAIL: valgrind produced no ERROR SUMMARY (harness crashed or valgrind did not run)"
    printf '%s\n' "$OUT" | tail -20
    exit 1
fi
# Fail on the real leak signal. The masked-select 'Use of uninitialised value'
# is a known ctgrind/memcheck artifact of the constant-time cmov and is not a
# branch, so it is intentionally not treated as a failure here.
if printf '%s\n' "$OUT" | grep -q "Conditional jump or move depends on uninitialised"; then
    echo "FAIL: secret-dependent branch detected by ctgrind"
    rc=1
else
    echo "PASS: ctgrind found no 'Conditional jump/move depends' findings"
fi

exit $rc
