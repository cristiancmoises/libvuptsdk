# ML-KEM-768 conformance suite (blocking CI gate)

Evidence tooling from the 2026-07-02 conformance fix. Wire into CI so a
non-conformant ML-KEM can never ship again. Acceptance = ACVP **80/80** AND both
differentials byte-equal in both directions AND the constant-time gate clean.

**Scope:** this gate exercises the **from-source** library (`src/zupt_mlkem.c`),
which is the code the fix corrected. It does **not** test the canonical
`prebuilt/libzuptsdk.so.2.0.0`, which predates the fix and is not rebuilt from
this source — regenerate and re-run against the prebuilt before claiming it
conformant. The 80 vectors are 25 keyGen + 25 encaps + 10 decaps + 10
encapsulationKeyCheck (§7.2) + 10 decapsulationKeyCheck (§7.3).

## Contents
- `kat_mlkem768_acvp.c` — deterministic ACVP KAT driver (seed-injecting stub).
- `vectors_*_mlkem768.json` — official NIST ACVP vectors (ML-KEM-768 slice of
  `usnistgov/ACVP-Server`, rev FIPS203), committed for offline reproducibility.
- `run_kats.py` — runs all 60 vectors through the driver.
- `kd_helper.c` + `build_kd.sh` — real-RNG CLI helper for the differentials.
- `differential_kyberpy.py` — bidirectional differential vs kyber-py.
- `differential_rustcrypto/` — bidirectional differential vs RustCrypto
  `ml-kem` 0.2.3 (the pin mirim trusts).
- CI job lives at `.forgejo/workflows/mlkem-conformance.yaml` (blocking, red-on-fail).

## Run locally (from a libvuptsdk checkout with this suite inside it)
```bash
gcc -O2 -Iinclude -Isrc conformance-suite/kat_mlkem768_acvp.c \
    src/zupt_mlkem.c src/zupt_keccak.c src/zupt_sha256.c -o katz
cd conformance-suite && python3 run_kats.py ../katz     # expect 80/80

sh conformance-suite/build_kd.sh                        # builds ./kd
python3 conformance-suite/differential_kyberpy.py ./kd  # expect 100/100 x2

cd conformance-suite/differential_rustcrypto && cargo build --release
# then drive both binaries as in differential_kyberpy.py (see report §6)
```

Constant-time gate (requires valgrind + gcc):
```bash
sh conformance-suite/ct/run_ct.sh    # dudect (blocking on |t|>=10) + ctgrind
```
