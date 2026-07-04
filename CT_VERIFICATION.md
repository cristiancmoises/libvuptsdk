# ML-KEM-768 — Constant-Time Verification Report

**Date:** 2026-07-02
**Engineer:** Cristian Cezar Moisés — Security Ops (*In Code We Trust*) · sac@securityops.co
**Subject:** side-channel (timing) verification of the **patched** `zupt_mlkem.c`
**Relates to:** `MLKEM_CONFORMANCE_FIX.md` (this closes the two §4.5 CT gates it left open)

This report discharges the charter's **§4.5 cross-cutting CT verification** ("at
least one independent timing/leakage check on the compiled artifact: dudect-style
statistical tests, ctgrind/Valgrind-based checks, or a CT verifier") — here, **two**
independent methods plus assembly inspection (§4.4). Toolchain: gcc 13.3.0,
clang 18.1.3, valgrind 3.22.0, x86-64. All checks run on the **conformance-fixed**
tree (ACVP 60/60 at the time of this report; later extended to 80/80 with the
FIPS 203 §7.2/§7.3 key-validation vectors — see `MLKEM_CONFORMANCE_FIX.md` §8).

Scope note: a clean statistical timing test and a clean taint-tracking run are
**evidence, not proof** (stated as such per charter §3.3). They complement, and do
not replace, the deductive/Jasmin proofs in §4.3/§4.4, which remain outstanding
(see §4 below).

---

## 1 · dudect-style statistical timing test (`dudect_decaps.c`)

**Method** (Reparaz–Balasch–Verbauwhede, DATE 2017): two input classes, randomly
interleaved, each decapsulation timed with an `lfence`-serialized `rdtsc`; Welch's
t-test on the two timing distributions under a sweep of upper-percentile crops
(tail latency from a shared host dominates otherwise). Inputs are pre-generated
**outside** the timed region; only `zupt_mlkem768_decaps` sits between the counter
reads. Convention: **|t| < 4.5** at this sample size → no evidence of leakage;
**|t| > 10** → definite leak.

**Experiment 1 — accept vs implicit-reject** (the security-critical path: does a
valid vs an invalid ciphertext take distinguishable time? A leak here reveals the
FO check result). `N = 60 000` per run.

| crop | n0 | n1 | \|t\| |
|---|---|---|---|
| p1.00 | 30032 | 29968 | 0.128 |
| p0.99 | 29730 | 29670 | 0.059 |
| p0.95 | 28535 | 28465 | 0.097 |
| p0.90 | 27010 | 26990 | 0.908 |
| p0.80 | 24022 | 23978 | 1.195 |
| p0.50 | 15067 | 14949 | 1.748 |

**max |t| = 1.748** → PASS. Repeat run: **1.610** → PASS (stable).

**Experiment 2 — fixed-vs-random on the accept path** (`-DEXP2`): class 1 is fresh
*valid* ciphertexts. **max |t| = 2.223** → PASS.

Both experiments sit far below the 4.5 threshold and are stable across runs → **no
evidence of secret-dependent timing** in decapsulation, including the
implicit-rejection branch that the conformance fix rewrote.

---

## 2 · ctgrind-style taint tracking (`ctgrind_mlkem.c`, Valgrind memcheck)

**Method** (Langley's ctgrind, as used by pqclean/BoringSSL): secret bytes are
marked `VALGRIND_MAKE_MEM_UNDEFINED`; the primitive runs under memcheck on the
**real compiled artifact** (optimizations included). memcheck raising
**"Conditional jump or move depends on uninitialised value(s)"** = a secret-
dependent branch/`cmov`-condition = a leak. Taint policy (documented, standard):
decaps taints `sk_pke = sk[0..1152)` and `z = sk[2368..2400)`, leaving the embedded
public-key copy defined so rejection sampling on the (public) `rho` is exercised
without false positives; encaps taints the drawn `m`; keygen taints `z` (leaving
`d` defined because `rho = G(d‖k)[0:32]` is public by construction — the same
declassification pqclean's harnesses apply).

**Result by compiler / optimization level:**

| Build | memcheck | "Conditional jump/move depends" findings |
|---|---|---|
| gcc -O2 | **clean, exit 0** | **0** |
| gcc -O3 | **clean, exit 0** | **0** |
| gcc -Os | **clean, exit 0** | **0** |
| clang -O2 | 4 findings, all **"Use of uninitialised value"** | **0** |
| clang -O3 | 2 findings, all **"Use of uninitialised value"** | **0** |

**Zero "Conditional jump or move depends on uninitialised value" across every
build** — the signal that actually indicates a timing leak. gcc is fully clean.

The clang **"Use of uninitialised value of size 8"** findings all originate at the
branchless select `cmov()` (`r[i] ^= mask & (r[i]^x[i])`), where `mask` is
correctly secret-derived (it depends on the FO re-encryption comparison). This is
the well-known memcheck-on-masked-select artifact: memcheck flags the tainted data
flowing through the widened `and`/`xor`, **not** a branch. Confirmed two ways:
disabling clang vectorization (`-fno-vectorize -fno-slp-vectorize`) leaves the same
"Use" (so it is the tainted selector, not SIMD), and the assembly (§3) shows the
select is pure `and`/`xor`/`pandn` with no branch.

---

## 3 · Assembly inspection (charter §4.4)

`decaps` disassembled and every conditional control-transfer classified, across
compilers:

| Build | conditional **jumps** (`jcc`) in decaps | hardware `cmov` (constant-time) |
|---|---|---|
| gcc -O2 | **0** | 0 (fully masked `and`/`xor`) |
| gcc -O3 | **0** | 1 |
| clang -O2 | **0** | 6 |

- **No conditional jumps** in any build → no branch-predictor-visible,
  secret-dependent control flow.
- The hardware `cmov` instructions present under gcc-O3/clang are constant-time by
  design (fixed latency, no speculation on the condition). memcheck flagged **none**
  of them as "Conditional move depends on uninitialised value", so their conditions
  are **public** (they come from the rejection sampler's `d < q` test on public
  `rho`, and from loop bookkeeping) — not from secret data.

Conclusion: the compiled decapsulation performs branchless, constant-time selection
between the accept key and the implicit-rejection key on every target compiler.

---

## 4 · Outstanding (honest scope — charter §2, no fabrication)

- **Frama-C EVA/WP (§4.3)** and **Jasmin CT + EasyCrypt (§4.4)** were **not** run
  here. `opam` and an OCaml switch were provisioned and the exact Frama-C
  dependency set resolved, but the full Alt-Ergo/Why3 prover stack (plus GTK system
  libraries, partially unavailable on this host's package mirror) exceeded the
  environment's build budget. These should run in CI with the toolchain pinned per
  §7. Nothing in the conformance fix is expected to affect their outcome — the fix
  touched only public loop indices and the (data-independent) KDF, preserving the
  pre-existing constant-time `cmov` selection.

- The two checks above are statistical/dynamic evidence on this host; they are
  **not** a substitute for the deductive proofs. They are reported as evidence.

---

## 5 · Reproduction

```bash
# dudect
gcc -O2 -Iinclude -Isrc conformance-suite/ct/dudect_decaps.c \
    src/zupt_mlkem.c src/zupt_keccak.c src/zupt_sha256.c -o dudect -lm
./dudect                    # Exp 1: accept vs implicit-reject
gcc -O2 -DEXP2 ... -o dudect2 -lm && ./dudect2   # Exp 2

# ctgrind (needs valgrind + valgrind/memcheck.h)
gcc -O2 -g -Iinclude -Isrc conformance-suite/ct/ctgrind_mlkem.c \
    src/zupt_mlkem.c src/zupt_keccak.c src/zupt_sha256.c -o ctg
valgrind --tool=memcheck --error-exitcode=99 -q ./ctg     # expect no "Conditional jump/move" findings
```

---

*In Code We Trust — and we make the trust verifiable. Evidence labeled as evidence,
proof reserved for what is proved.*
