# libvuptsdk — ML-KEM-768 Conformance Fix

**Date:** 2026-07-02
**Engineer:** Cristian Cezar Moisés — Security Ops (*In Code We Trust*) · sac@securityops.co
**Component:** `src/zupt_mlkem.c` (ML-KEM-768, FIPS 203)
**Status change:** ⛔ non-conformant (0/60 ACVP) → ✅ **conformant (60/60 ACVP, interoperable)**
**Coverage since extended to 80/80** (adds FIPS 203 §7.2/§7.3 key-validation vectors) and scoped to the from-source build — see §8.

This report follows the Security Ops charter (§1 Correctness is prime directive
#1, §5 KAT + differential, §6 auditability). Every result below was produced by
a command run against the source in this repository; toolchain: gcc 13.3.0,
Python 3.12.3, kyber-py 1.2.0, RustCrypto `ml-kem` 0.2.3 (rustc 1.96.1).

---

## 1 · Summary

The library's ML-KEM-768 was **self-consistent but not FIPS 203 conformant**:
its own encaps/decaps roundtripped, yet it failed **all 60** official NIST ACVP
vectors and could not interoperate with any standard implementation. Two
independent defects were found, isolated to their exact layer, and fixed with a
minimal patch. Post-fix: **60/60 ACVP**, plus byte-for-byte agreement with
**two** independent implementations in both directions.

Both defects were in the **wrapper**, not the heavy math: the NTT, base
multiplication, Montgomery reduction, CBD sampling, rejection sampling, SHA-3/
SHAKE, and byte (de)coding were all already correct (proven by the fix requiring
no change to any of them).

---

## 2 · Defect 1 — matrix generated transposed (K-PKE, keygen + encrypt)

### Root cause
FIPS 203 samples the matrix entry `Â[i][j]` from `SampleNTT(XOF(ρ, j, i))` —
the two index bytes are appended in **(j, i)** order (confirmed against the
kyber-py reference: `_xof(rho, bytes([j]), bytes([i]))`). The library appended
**(i, j)**:

```c
poly_uniform(Ahat[i][j], rho, (uint8_t)i, (uint8_t)j);   // WRONG: (i,j)
```

This transposes `Â` relative to the standard. Off-diagonal entries are swapped,
so `t̂ = Â∘ŝ + ê` is a different vector with **no per-coefficient relationship**
to the correct one — which is exactly the signature observed during diagnosis
(64 distinct coefficient ratios across 64 coefficients, i.e. unrelated vectors,
ruling out a Montgomery-domain scaling bug).

### Isolation evidence
- `ρ` (public seed) matched the reference **byte-for-byte** → `G(d‖k)`, SHA-3,
  and domain separation were correct; the divergence was downstream.
- The rejection sampler itself was **byte-exact**: for identical appended bytes,
  `poly_uniform(ρ, a, b)` produced the same coefficients as
  kyber-py's `ntt_sample(XOF(ρ, a, b))` (e.g. `2512 1187 75 2259 …`).
- `ntt()` and the CBD noise sampler both matched kyber-py **mod q**.
- Therefore the only possible error in `Â` was the index-byte placement.

### Fix
Append `(j, i)` in keygen; and in encrypt, where `A^T` is used
(`(A^T)[i][j] = A[j][i]`, appended `(i, j)`), append `(i, j)`:

```c
poly_uniform(Ahat[i][j], rho, (uint8_t)j, (uint8_t)i);   // keygen  → (j,i)
poly_uniform(AT[i][j],   rho, (uint8_t)i, (uint8_t)j);   // encrypt → (i,j)
```

After this fix alone: **keyGen 25/25**, and the K-PKE ciphertext (`u` and `v`)
matched kyber-py **byte-for-byte** — proving the encrypt path correct too.

---

## 3 · Defect 2 — pre-final Kyber KDF instead of FIPS 203 shared secret

### Root cause
After Defect 1, the ciphertext was correct but the **shared secret** still
differed. The library derived it with the **round-3 Kyber** construction:

```c
/* encaps */  ss = SHAKE256( K ‖ H(c) )
/* decaps */  K'  = SHAKE256( K ‖ H(c) )        // success
              K̄  = SHAKE256( z ‖ H(c) )        // rejection
```

In the **final FIPS 203**, NIST **removed the final hash**. The shared secret is
`K` straight out of `G(m ‖ H(ek))`, and the implicit-rejection value is
`J(z ‖ c)` over the **full ciphertext** — not `H(c)`. Confirmed against
kyber-py (`_encaps_internal` returns `K` directly; `_decaps_internal` uses
`J(z ‖ c)`).

### Fix (FIPS 203 Alg 17/18)
```c
/* encaps */  memcpy(ss, kr, 32);                       // K = kr[0:32] directly
/* decaps */  memcpy(ss_success, kr, 32);               // K' = kr[0:32]
              ss_reject = SHAKE256( z ‖ c )             // J(z ‖ c), full 1088-byte c
```

The constant-time `cmov` selection between success and rejection keys is
**unchanged**; both values are still always computed, so no secret-dependent
branch or timing was introduced by the fix.

---

## 4 · Evidence after fix (the charter §5 gate)

| Check | Method | Result |
|---|---|---|
| **Official NIST ACVP KATs** | `usnistgov/ACVP-Server` rev FIPS203, ML-KEM-768 | **60/60** (keyGen 25, encaps 25, decaps 10 incl. implicit rejection) |
| **Interop (regression of the original break)** | kyber-py encaps → SDK decaps | shared secret **matches** (was mismatched pre-fix) |
| **Differential vs kyber-py 1.2.0** | 100 random each direction | **100/100** and **100/100** |
| **Differential vs RustCrypto `ml-kem` 0.2.3** (mirim's pin) | 50 random each direction | **50/50** and **50/50** |
| **Property (self-roundtrip)** | 1000 random `decap(encap)==ss` | **1000/1000** |
| **Negative (tampered ciphertext)** | 1000 random single-bit flips | **1000/1000** rejected, **no crash** (implicit rejection defined) |
| **ASan + UBSan** | roundtrip harness + decap KATs | **0 errors**, exit 0 |
| **Library regression** | `make test` (smoke + ABI/symbol audit) | **10/10** smoke, **13/13** audit |
| **High-level hybrid path** | `easy_encrypt/decrypt` (ML-KEM-768 + X25519) roundtrip, wrong-key, tamper | **PASS** |

The exact same harness design produced 60/60 for a known-good ML-KEM
implementation (keywave's provider), so the method is calibrated, not lenient.

---

## 5 · Constant-time note (honest scope)

The fix introduces **no** secret-dependent branch, index, or variable-latency
operation: the matrix change touches only public loop indices; the KDF change is
`memcpy` + Keccak (data-independent by construction) with the pre-existing
constant-time `cmov` selection preserved. Two independent CT checks were subsequently run on the patched artifact and
**passed** — dudect-style statistical timing (max |t| = 1.7, threshold 4.5) and
ctgrind taint-tracking (zero "Conditional jump/move depends on uninitialised"
findings; gcc fully memcheck-clean), plus assembly inspection showing **zero
conditional jumps** in `decaps` across gcc/clang. Full methodology and results in
**`CT_VERIFICATION.md`**, with harnesses in `conformance-suite/ct/`. The
repository's Frama-C WP / EasyCrypt / Jasmin proofs remain outstanding (heavy
prover stack not provisionable in this environment) and should run in CI on the
patched tree; nothing in the change is expected to affect the CT properties.

---

## 6 · Why this shipped, and the fix that matters most

An ML-KEM that fails 60/60 KATs is impossible to ship past a **KAT gate** — the
absence of that gate is the true root cause. The most important deliverable here
is therefore not the two-line arithmetic/KDF change; it is the **conformance
suite** shipped alongside this report (`conformance-suite/`), ready to wire into
`.forgejo/workflows` as a **blocking** CI gate:

- `kat_mlkem768_acvp.c` — deterministic ACVP KAT driver (overrides
  `zupt_random_bytes` to inject vector seeds).
- `differential_kyberpy.py` — bidirectional differential vs kyber-py.
- `differential_rustcrypto/` — bidirectional differential vs RustCrypto
  `ml-kem` 0.2.3 (the pin mirim already trusts).

**Acceptance criterion (make it the gate):** ACVP 60/60 **and** both
differentials byte-equal in both directions. This defect class cannot recur once
that gate is red-on-fail.

---

## 7 · Charter Definition-of-Done status for this change

- [x] KATs match official vectors bit-for-bit (60/60); differential passes (2 impls, both directions).
- [x] Property + negative tests green.
- [x] ASan/UBSan clean.
- [x] No secret-dependent branch/index introduced; CT `cmov` preserved.
- [x] Constant-time verified by tooling (dudect + ctgrind + asm inspection) — see `CT_VERIFICATION.md`.
- [x] Library regression (`make test`) green; hybrid path validated.
- [x] Fix documented with root cause, evidence, and reproduction (this file).
- [ ] Frama-C WP / EasyCrypt / Jasmin CT re-run — **outstanding**, run in CI on patched tree (not available in this environment; not fabricated).
- [ ] `cargo`-side gates N/A (C component); SBOM/release-signing handled at repo level.

---

## 8 · Addendum (2026-07-04) — scope, key-validation, and migration

Follow-up work after the original report, integrated into this repository:

- **ACVP coverage extended 60 → 80.** The suite now also runs the 10
  `encapsulationKeyCheck` (FIPS 203 §7.2 modulus check) and 10
  `decapsulationKeyCheck` (§7.3 hash check) vectors that the first pass skipped.
  `zupt_mlkem768_encaps` now performs the §7.2 check and rejects malformed
  encapsulation keys; `zupt_mlkem768_check_ek` / `_check_dk` expose the checks.
  Result: **80/80**. Valid keys are unaffected (0/1000 valid keys rejected).
- **Secret-hygiene hardening.** ML-KEM stack temporaries that previously
  survived a call (`d‖k` seed, message/noise polynomials `m`,`w`,`u`,`v`) are
  now wiped; one-shot SHA3/SHAKE wipe their sponge state after squeezing.
- **Constant-time portability caveat (disclosed, not hidden).** The polynomial
  compression divides/reduces by `q` on secret-derived data (KyberSlash-class,
  CVE-2024-37880 family). On x86-64 gcc/clang this is compiled to constant-time
  multiply-by-reciprocal — which is why the dudect + ctgrind results above are
  clean — but the C source is not constant-time-portable to targets that emit a
  hardware variable-latency divide. Multiply-shift hardening is tracked.

### Scope: from-source vs the shipped prebuilt

This fix and all evidence above pertain to the **from-source** library
(`libvuptsdk-base.so`, built from `src/`). The canonical
`prebuilt/libvuptsdk.so.2.0.0` is a binary that predates the fix and is **not**
rebuilt from this patched tree; it must be regenerated and re-audited before it
can be claimed FIPS 203 conformant. The status line in §1 (⛔→✅) therefore
describes the source implementation, not the prebuilt artifact.

### Migration hazard (operational)

The corrected KEM derives a **different, now-standard** shared secret than the
pre-fix code. Consequences for existing data:

- An ML-KEM-hybrid archive or keypair produced by a **pre-fix** build will not
  decrypt on a **post-fix** build: decapsulation re-derives the corrected
  matrix/secret, the AEAD MAC fails, and the archive reads as tampered /
  wrong-key. The reverse (post-fix archive on a pre-fix build) fails the same way.
- **Remediation:** re-encrypt affected data with a post-fix build. There is no
  in-place upgrade — the pre-fix ciphertexts were never FIPS 203 ciphertexts.

*In Code We Trust — and we make the trust verifiable. Including when it says the KEM was wrong, and again when it says it's right.*
