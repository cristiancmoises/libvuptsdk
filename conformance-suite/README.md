# ML-KEM-768 conformance suite

Evidence tooling from the 2026-07-02 conformance fix. A release candidate
passes this suite when ACVP is **80/80**, both differentials are byte-equal in
both directions, and the constant-time gate is clean. Repository owners must
separately configure CI jobs as required checks before merging or releasing.

**Scope:** this gate exercises the **from-source** library (`src/zupt_mlkem.c`),
which is the code the fix corrected. It does **not** test the canonical
`prebuilt/libvuptsdk.so.2.0.3`, which predates the fix and is not rebuilt from
this source — regenerate and re-run against the prebuilt before claiming it
conformant. The 80 vectors are 25 keyGen + 25 encaps + 10 decaps + 10
encapsulationKeyCheck (§7.2) + 10 decapsulationKeyCheck (§7.3).

## Contents
- `kat_mlkem768_acvp.c` — deterministic ACVP KAT driver (seed-injecting stub).
- `vectors_*_mlkem768.json` — official NIST ACVP vectors (ML-KEM-768 slice of
  `usnistgov/ACVP-Server`, rev FIPS203), committed for offline reproducibility.
- `run_kats.py` — runs all 80 checks through the driver.
- `kd_helper.c` + `build_kd.sh` — real-RNG CLI helper for the differentials.
- `differential_kyberpy.py` — bidirectional differential vs kyber-py.
- `differential_rustcrypto/` — bidirectional differential vs RustCrypto
  `ml-kem` 0.2.3 (the pin mirim trusts).
- CI jobs for ACVP, kyber-py and constant-time checks live at
  `.forgejo/workflows/mlkem-conformance.yaml`; the RustCrypto differential is
  part of the documented local release gate.

## Run locally (from a libvuptsdk checkout with this suite inside it)
```bash
gcc -O2 -Iinclude -Isrc conformance-suite/kat_mlkem768_acvp.c \
    src/zupt_mlkem.c src/zupt_keccak.c src/zupt_sha256.c -o katz
cd conformance-suite && python3 run_kats.py ../katz     # expect 80/80

sh conformance-suite/build_kd.sh                        # builds ./kd
python3 conformance-suite/differential_kyberpy.py ./kd  # expect 100/100 x2

cargo build --release --locked \
    --manifest-path conformance-suite/differential_rustcrypto/Cargo.toml
python3 conformance-suite/differential_rustcrypto.py ./kd \
    conformance-suite/differential_rustcrypto/target/release/rustdiff768
# expect 100/100 in both directions
```

Constant-time gate (requires valgrind + gcc):
```bash
sh conformance-suite/ct/run_ct.sh    # dudect (failure at |t|>=10) + ctgrind
```
