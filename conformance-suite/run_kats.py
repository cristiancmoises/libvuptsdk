#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Run the official NIST ACVP ML-KEM-768 KATs against the compiled `katz`
driver over the committed vectors. Exit 0 iff every committed vector runs AND
passes. The suite covers all five ACVP functions: keyGen, encapsulation,
decapsulation, encapsulationKeyCheck (FIPS 203 §7.2), and decapsulationKeyCheck
(FIPS 203 §7.3) — 80 vectors total.

A guard asserts the number of vectors actually executed matches the number
committed, so a truncated/empty/renamed vector file fails loudly instead of
printing "0/0 pass" and exiting 0."""
import json, subprocess, sys

KATZ = sys.argv[1] if len(sys.argv) > 1 else "./katz"

def run(*a):
    r = subprocess.run([KATZ, *a], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"katz usage error (exit {r.returncode}) on mode {a[:1]}: {r.stderr.strip()}")
    return r.stdout.strip()

def load(fn):
    with open(fn) as fh:
        return json.load(fh)

p = f = executed = 0

for g in load("vectors_keygen_mlkem768.json")["testGroups"]:
    for t in g["tests"]:
        r = run("keygen", t["d"], t["z"], t["ek"], t["dk"])
        executed += 1; p += (r == "1"); f += (r != "1")

for g in load("vectors_encapdecap_mlkem768.json")["testGroups"]:
    fn = g["function"]
    for t in g["tests"]:
        if fn == "encapsulation":
            r = run("encap", t["ek"], t["m"], t["c"], t["k"])
        elif fn == "decapsulation":
            r = run("decap", t["dk"], t["c"], t["k"])
        elif fn == "encapsulationKeyCheck":
            r = run("encapCheck", "1" if t["testPassed"] else "0", t["ek"])
        elif fn == "decapsulationKeyCheck":
            r = run("decapCheck", "1" if t["testPassed"] else "0", t["dk"])
        else:
            sys.exit(f"unknown ACVP function in vectors: {fn!r}")
        executed += 1; p += (r == "1"); f += (r != "1")

EXPECTED = 80  # 25 keyGen + 25 encaps + 10 decaps + 10 encapKeyCheck + 10 decapKeyCheck
print(f"ACVP ML-KEM-768: {p}/{executed} pass ({f} fail)")
if executed != EXPECTED:
    sys.exit(f"FAIL: executed {executed} vectors, expected {EXPECTED} "
             f"(vector files truncated, empty, or schema changed)")
sys.exit(0 if f == 0 else 1)
