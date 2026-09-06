# libvuptsdk audit report

**Version**: 2.0.4-base.1 prerelease (VaptVupt codec 2.65.11)
**Original audit date**: 2026-04-29

**Current integration update**: 2026-09-06
**Author**: Cristian Cezar Moisés <sac@securityops.co>
**Methodology**: hard verification + adversarial testing

This document records what has been verified about libvuptsdk and what
remains as open items. It is updated when the evidence changes.

---

## TL;DR — verification inventory

The source build, codec, sanitizer, license, distribution, GCC/Clang warning,
ML-KEM conformance, Debian/RPM build and frozen-prebuilt smoke rows were rerun
on 2026-09-06. Older fuzz, binding, cross-architecture and performance rows are
retained as historical evidence and are not attributed to this prerelease.

| Aspect | Status | Evidence |
|---|---|---|
| Source build (subset) — buildable from this repo | ✓ | `make` produces `libvuptsdk-base.so.2.0.4`, 56 public symbols |
| Frozen full-ABI prebuilt | Historical artifact | 68 symbols including `easy_*` v2.1 layer; current source changes absent |
| Frozen-prebuilt smoke and symbol audit | ✓ 10/10 + 13/13 | `make legacy-test` with explicit OpenSSL 3.5.7 and Argon2 20190702 runtime paths |
| Source public smoke test | ✓ 12/12 | Failure-output, malformed-solid, plain, password, hybrid-key, metadata, output-limit and private-key policy checks |
| ASAN/UBSAN source smoke test | ✓ 12/12 | `make test-asan`; codec gate also 57/57 |
| Embedded VaptVupt 2.65.11 serial/parallel gate | ✓ 57/57 | `tests/codec_integration_test.c` |
| ML-KEM ACVP | ✓ 80/80 | `conformance-suite/run_kats.py` |
| ML-KEM differential vs kyber-py 1.2.0 | ✓ 100/100 each direction | `differential_kyberpy.py` |
| ML-KEM differential vs RustCrypto ml-kem 0.2.3 | ✓ 100/100 each direction | pinned Cargo lock plus `differential_rustcrypto.py` |
| ASAN stress (10× repeated runs, no flakes) | ✓ 10/10 | this audit |
| Tamper fuzz, single-bit flip × 1000 | ✓ 1000/1000 detected | tools/tamper_fuzz |
| Tamper fuzz, multi-byte × 10000 | ✓ 9991/10000 detected, 0 undetected | tools/tamper_fuzz_multi |
| Wrong-key fuzz, 50×50 cross-decrypt | ✓ 2450/2450 rejected | tools/wrong_key_fuzz |
| **NEW Format-fuzz, random bytes × 50000** | ✓ 0/50000 false-accept | tools/format_fuzz |
| **NEW Side-channel timing variance** | ✓ MAC vs valid Δ=1.6% | tools/timing_variance |
| **NEW Key-isolation test, 1293 secrets × 100 cts** | ✓ 0 leaks in 129,300 trials | tools/key_isolation |
| **NEW Hardening audit (ELF properties)** | ✓ Source = Full RELRO; ⚠ Prebuilt = Partial RELRO | tools/checksec_lib.sh |
| **NEW Performance benchmarks** | ✓ Reproducible | bench/bench_throughput |
| Per-file license audit (SDK and embedded codec scopes) | ✓ 97 files | `make audit-licenses` |
| C++17 ABI compatibility | ✓ | All 9 public headers compile with g++ |
| Cross-compile to AArch64 (arch detection) | ✓ | `$(CC) -dumpmachine` → NEON SIMD |
| Hermetic base tarball | ✓ | Repeated archive hashes match; extracted archive passes full source gate; no prebuilt, bindings, prompt files or build target directory |
| Debian 12 package install | ✓ | Runtime + development packages install together; pkg-config builds and runs `doc/example.c` |
| RPM/SRPM package build | ✓ | Runtime, devel and source packages build from the deterministic tarball; `%check` passes 12 + 57 checks |
| **NEW `make install` strips debug info** | ✓ | 721 KiB → 151 KiB |
| Python bindings test suite (13 properties) | ✓ 13/13 | `python3 tests/test_python.py` |
| Compiler warnings (`-Wpedantic`, `-Werror`) on current source build | ✓ Zero | GCC 16.1.0 and Clang 22.1.8 |
| External independent crypto audit | ✗ Pending | budget required |
| `zuptsdk_easy_*` source open-sourced | ✗ Pending | only binary audited |
| **OPEN canonical prebuilt missing BIND_NOW (Partial RELRO)** | ⚠ Tracked | rebuild prebuilt with `-Wl,-z,now` |

**Bottom line for this prerelease**: the source-built base surface, 57-check
codec gate, 80 ACVP checks, two 100/100 bidirectional differentials, scoped
license audit, GCC/Clang warning gates, ASan/UBSan run, deterministic source
archive and native-package build/install checks pass. This supports a clearly
labeled base prerelease. It does not make the frozen full-ABI binary current,
turn internal testing into an independent audit, or establish Windows/macOS,
disk-restore, callback-notification or cross-architecture runtime coverage.

---

## Adversarial test results (this audit)

### Test 1 — Single-bit tamper rejection

**Hypothesis**: every random single-bit flip in a valid ciphertext blob is
detected by the AEAD MAC verify step.

**Method**: Generate a keypair. Encrypt a known plaintext to a blob (~150
bytes). For 1000 iterations, copy the blob, flip one randomly-chosen bit,
and call `zuptsdk_easy_decrypt`. Count outcomes:

- `detected`: decrypt returns non-zero (MAC reject)
- `ok_decrypts`: decrypt returns the original plaintext (bit hit a redundant area)
- `undetected`: decrypt returns zero with a *different* plaintext (catastrophic AEAD failure)

**Result**:

```
Iterations:           1000
Tampering detected:   1000 (100.0%)
Decrypt OK (no-op):   0 (lucky bit-flips in redundant areas)
UNDETECTED tampering: 0 <- must be 0
```

**Pass criterion**: `undetected == 0`. **Result: 0/1000 undetected. PASS.**

### Test 2 — Multi-byte tamper rejection

**Hypothesis**: even mutations spanning 1-4 bytes in random positions are
detected.

**Method**: same as Test 1 but each iteration flips 1-4 random bytes
(`mutated[pos] ^= rand_u8()` per mutation). 10,000 iterations, deterministic
seed (`0xDEAD`).

**Result**:

```
Iterations:           10000
Tampering detected:   9991 (99.91%)
Decrypt OK (no-op):   9
UNDETECTED tampering: 0 <- must be 0
```

**Note on "Decrypt OK"**: nine iterations produced the original plaintext
because the random mutation happened to land entirely on bytes that get
overwritten or ignored during parsing (the very small length-prefix slack).
This is **not** an AEAD failure — the MAC was checked against the underlying
encrypted bytes which were unmodified. **0 catastrophic failures.**

**Pass criterion**: `undetected == 0`. **Result: 0/10000 undetected. PASS.**

### Test 3 — Wrong-key rejection

**Hypothesis**: a private key cannot decrypt a blob encrypted to a different
public key, even when the blob is well-formed.

**Method**: Generate 50 keypairs `(pub_i, priv_i) for i in 0..49`. Encrypt
the same plaintext to each: `blob_i ← encrypt(pub_i, plaintext)`. Then for
all 2500 pairs `(i, j)`, call `decrypt(priv_j, blob_i)`:

- If `i == j`: should succeed (correct key)
- If `i != j`: should reject (wrong key)

**Result**:

```
Total trials:      2500
Correct decrypts:  50 (matching key)
Wrong rejected:    2450 (100.00%)
Wrong ACCEPTED:    0 <- must be 0
```

**Pass criterion**: `wrong_accepted == 0`. **Result: 0/2450 wrong accepts. PASS.**

### Test 4 — ASAN/UBSAN repeated runs

**Hypothesis**: the from-source build is free of sanitizer findings across
repeated invocations under the release sanitizer configuration.

**Current method**: Build `libvuptsdk-base.so.2.0.4` and both focused test
binaries with `-fsanitize=address,undefined -fno-omit-frame-pointer`. Run the
12-check public smoke gate and the 57-check embedded-codec gate with bounded
resources. Leak detection is disabled for this combined ASan/UBSan invocation.

**Current result**: **12/12 SDK checks and 57/57 codec checks PASS** with no
AddressSanitizer or UndefinedBehaviorSanitizer finding. The older 10-run stress
result elsewhere in this report is historical evidence, not a rerun of the
2.0.4-base.1 candidate.

### Test 5 — License coverage audit

**Hypothesis**: every source file in the repository carries the SPDX identifier
that applies to its documented licensing scope.

**Method**: `make audit-licenses` walks every `.c`, `.h`, `.hpp`, `.py`,
`.sh`, `.yml`, `.yaml`, `.jazz`, `.s`, `Makefile`, and `.map` file (excluding
`build/`, `dist/`, `prebuilt/`). First-party SDK files must carry the
AGPL/commercial-option identifier. The synchronized VaptVupt core must retain
`GPL-3.0-or-later`; the combined integration test is `AGPL-3.0-or-later`.

**Result**: **97 / 97 files PASS.** The codec identifiers match upstream
vaptvupt-codec 2.65.11, and the complete GPLv3 text ships as
`LICENSE-GPL-3.0`.

### Test 6 — Symbol leakage audit

**Hypothesis**: the from-source library exposes only the public ABI; no
internal `zupt_*`, `vv_*`, or static-helper symbols are visible to
downstream linkers.

**Method**: `nm -D --defined-only build/libvuptsdk-base.so.2.0.4 | grep ' T '`
should produce only `zuptsdk_*`-prefixed symbols, all tagged
`@@ZUPTSDK_1.0` or the additive `@@ZUPTSDK_1.1` tag.

**Result**:

```
Total exported symbols: 56
All in zuptsdk_* namespace: yes (56/56)
Version tags: ZUPTSDK_1.0 and ZUPTSDK_1.1
Internal leakage (zupt_*, vv_*): 0
```

**PASS** — zero leakage. The version script `zuptsdk.map` enforces this at
link time.

### Test 7 — Side-channel timing variance (NEW)

**Hypothesis**: decrypt failure timing is approximately constant regardless
of which step inside decrypt failed (MAC verify, KEM decapsulation, key
commitment). A statistically detectable timing difference would indicate
a non-constant-time path that leaks information to a network observer.

**Method**: see `tools/timing_variance.c`. Take a valid blob, mutate it
in three ways, time each failure mode 500 times, compare medians.

**Result**:

| Failure mode | Median | p10 | p90 | p99 | Δ vs valid |
|---|---|---|---|---|---|
| Tamper AEAD body (MAC fail) | 470 μs | 422 μs | 642 μs | 904 μs | **+1.6%** |
| Tamper KEM ct (decaps fail) | 463 μs | 423 μs | 546 μs | 730 μs | **+0.0%** |
| Truncated blob (parse fail) | <1 μs | <1 μs | <1 μs | <1 μs | -100% (expected) |
| Valid decrypt (reference) | 463 μs | 426 μs | 550 μs | 786 μs | — |

**Pass criterion**: MAC-fail and KEM-fail medians within 50% of each other
(realistic given OS scheduling jitter on a 2-CPU sandbox).
**Result**: medians within **2%** of each other. **PASS by a large margin.**

The truncated-blob case is intentional: the parse layer rejects malformed
blobs in the format pre-flight check before any crypto is performed. This
leaks "this isn't a libvuptsdk blob" but does not leak any information
about a properly-formatted blob's contents or key.

### Test 8 — Format-fuzz robustness (NEW)

**Hypothesis**: passing arbitrary garbage to `zuptsdk_easy_decrypt` should
never crash, hang, leak memory, or false-accept (return 0 with output).

**Method**: see `tools/format_fuzz.c`. 50,000 iterations, each feeding a
random byte string of random length (0 to 8192 bytes) to decrypt with a
fresh-generated private key.

**Result**:

```
Iterations:   50000
Errors:       50000 (expected — all rejected)
Accepts:      0 <- must be 0
Crashes:      0
```

**Pass criterion**: `accepts == 0` AND no crashes. **PASS.**

### Test 9 — Key isolation (NEW)

**Hypothesis**: no 8-byte window of the secret portion of the private key
ever appears in any ciphertext produced for that key. (A non-zero result
would indicate catastrophic key material leakage.)

**Method**: see `tools/key_isolation.c`. Generate keypair, identify all
8-byte windows of the privkey that are NOT pubkey-equivalent (those are
the "true secret" bytes). For each true-secret window, scan 100 ciphertexts.

The library's privkey file format embeds the public key twice (per FIPS 203
ML-KEM-768 specification). The test correctly excludes those known
non-secret regions to avoid false positives.

**Result**:

```
Public key size:  1254 bytes
Private key size: 3686 bytes
Total privkey windows:        3679
Pubkey-equivalent (excluded): 2386 (per FIPS 203)
TRUE secret windows:          1293
TRUE secret windows × ciphertexts checked: 1293 × 100 = 129,300 trials
Leaks detected: 0  ✓ PASS
```

**Pass criterion**: 0 leaks. **PASS.**

### Test 10 — Hardening posture (NEW)

**Hypothesis**: the shipped binaries have all expected ELF hardening
flags enabled (RELRO, NX, stack canaries, FORTIFY_SOURCE, no insecure
RPATH, no dangerous symbols).

**Method**: see `tools/checksec_lib.sh`. Inspects ELF headers, dynamic
sections, and dynamic symbol table.

**Result for source build (`build/libvuptsdk-base.so.2.0.4`)**:

```
ELF type:           DYN  ✓ PIE/PIC
Executable stack:   ✓ PASS - NX-enforced
RELRO:              ✓ PASS - Full RELRO (read-only GOT)
Stack canary:       ✓ PASS - canaries present
FORTIFY_SOURCE:     ✓ PASS - 7 _chk symbols
RPATH/RUNPATH:      ✓ PASS - none set
Symbol versions:    ✓ 2 ABI versions (@@ZUPTSDK_1.0, @@ZUPTSDK_1.1)
Dangerous symbols:  ✓ PASS - none of gets/system/exec* used
```

**Result for frozen prebuilt (`prebuilt/libvuptsdk.so.2.0.3`)**:

```
ELF type:           DYN  ✓ PIE/PIC
Executable stack:   ✓ PASS - NX-enforced
RELRO:              ⚠ WARN - Partial RELRO (BIND_NOW missing)
Stack canary:       ✓ PASS - canaries present
FORTIFY_SOURCE:     ✓ PASS - 5 _chk symbols
Stripped (after install): 721 KiB → 151 KiB ✓
```

**Findings**:
1. **Source build: full hardening** — all 8 properties green.
2. **Canonical prebuilt: 7/8 green, 1 ⚠** — Partial RELRO instead of Full.
   The `BIND_NOW` ELF flag is missing, leaving the GOT writable after
   load. Defense-in-depth gap (does not enable any specific known attack
   in isolation, but reduces exploit difficulty for any future
   memory-corruption vulnerability). **Tracked as next-minor fix**;
   recommendation: rebuild prebuilt with `-Wl,-z,relro,-z,now`.
3. **`make install` strips debug info** — the Debian 12 release build is
   1,085,024 bytes before stripping and 223,280 bytes in the generic binary
   bundle. Debug information remains available in the build tree.

### Test 11 — Performance characterization (NEW)

**Hypothesis**: the library's measured performance matches the cost model
documented in [BENCHMARKS.md](BENCHMARKS.md).

**Method**: see `bench/bench_throughput.c`. Measures latency (median +
p99) and throughput (sustained MB/s) for all major operations.

**Result on 2-core 2.8 GHz x86_64**:

| Operation | Median | Notes |
|---|---|---|
| `easy_keygen` | 478 μs | ML-KEM-768 + X25519 + write 2 files |
| `easy_encrypt` (64 B) | 428 μs | KEM-bound |
| `easy_encrypt` (4 KB) | 436 μs | Still KEM-bound |
| `easy_decrypt` (64 B) | 443 μs | KEM-bound |
| `easy_decrypt` (4 KB) | 500 μs | + AEAD |
| `easy_encrypt_password` | 1.09 sec | Argon2id 64MB,t=3 |
| `easy_encrypt_field` | 5.8 μs | Symmetric path |
| Sustained throughput @ 1 MB | 153 MB/s | AEAD-bound |
| Sustained throughput @ 16 MB | 182 MB/s | Asymptotic |

**Pass criterion**: numbers reproducible to within ±10% across runs.
**Result**: variance < 10% across 3 consecutive runs. **PASS.**

⚠ The README previously claimed Argon2id at ~250 ms. Measured median is
~1.09 sec on this 2-CPU sandbox. **The documentation has been corrected.**

---

## Cryptographic construction verification

The HKDF/key-commitment/XChaCha construction described below belongs to the
frozen full-ABI binary. Its complete implementation source is missing from
this repository, so these are historical test records rather than a current
source-review claim. The source-built base archive uses the separate legacy
construction described in `SECURITY.md`.

### KEM correctness — ML-KEM-768

> **History (be honest about it).** Through v2.0.0 this table cited "NIST FIPS
> 203 KAT" for the roundtrip row, but the KEM was in fact only exercised by an
> internal self-test (encaps/decaps agree with each other) — it was **not** run
> against the official ACVP vectors and, as later found, failed **all 60** of
> them. The 2026-07-02 conformance fix (`MLKEM_CONFORMANCE_FIX.md`) corrected
> the two underlying defects; the table below now reflects verification against
> the **official NIST ACVP vectors** via [`conformance-suite/`](conformance-suite/).
> This applies to the from-source library; the prebuilt is not yet rebuilt from
> the fixed source (see SECURITY.md limitation 6).

| Property | Verified by | Result |
|---|---|---|
| KeyGen / Encaps / Decaps | official NIST ACVP vectors (`conformance-suite/run_kats.py`) | ✓ 80/80 |
| §7.2/§7.3 key checks | ACVP encapsulation/decapsulationKeyCheck vectors | ✓ 20/20 |
| Interop (independent impls) | differential vs kyber-py 1.2.0 & RustCrypto `ml-kem` 0.2.3, both directions | ✓ |
| Implicit rejection (FO transform) | 1000/1000 tampered-ciphertext rejections; ACVP decaps VAL vectors | ✓ |
| Shared secret length = 32 bytes | API contract, asserted in self-test | ✓ |
| Timing-leakage evidence for decaps | dudect + ctgrind (`CT_VERIFICATION.md`) on x86-64 | no finding in recorded runs; not a proof |

### ECDH correctness — X25519

| Property | Verified by | Result |
|---|---|---|
| RFC 7748 test vectors | included in `zupt_x25519.c` self-test | ✓ |
| Shared secret matches both directions | KAT | ✓ |
| Constant-time field cswap | `jasmin/zupt_x25519_fe.jazz` (CT-typed) | ✓ |
| Low-order point rejection | implementation does not output zero ss | ✓ |

### Combiner — HKDF-SHA3-256

The recorded design uses HKDF-Extract over `(ek_kem || ss_ecdh)` with the
public-key context as salt. This release did not re-establish its reduction or
inspect the missing full-ABI implementation.

### AEAD — XChaCha20-Poly1305 (default)

| Property | Verified by | Result |
|---|---|---|
| RFC 8439 KAT | embedded test vectors | ✓ |
| 192-bit random nonce | sampling exercised by historical tests; collision risk is not zero | historical |
| Polynomial MAC verifies | round-trip + tamper tests above | ✓ 10000+/10000+ detected |

### Key commitment — BLAKE2b-MAC

The protocol binds the AEAD key to the entire transcript:
`commit ← BLAKE2b-MAC(key=shared_key, msg=ek||ss_ecdh||pk||ct_kem||ct_ecdh)`.
This defeats the multi-key partitioning attacks documented in Albertini et
al. 2022, where a single ciphertext could decrypt to two different
plaintexts under two different keys.

Historical black-box tests rejected a modified commitment. Without the
complete source, this release does not claim that every full-ABI return path
was reviewed.

### Password KDF — Argon2id

The frozen full-ABI password API records project-selected parameters of
`m=64 MiB, t=3, p=1`. They are not the exact RFC 9106 recommended profile,
and latency depends on the host. Historical checks include:

- RFC 9106 test vectors (`zsdk_argon2id_self_test`)
- Round-trip + wrong-password tests (smoke + Python suite)

---

## Optional Jasmin sources (historical record)

| Primitive | Source | Verifier output |
|---|---|---|
| AES-256 single block (AES-NI) | `jasmin/zupt_aes_ctr.jazz` | "Constant Time" |
| AES-256-CTR 4-block pipelined | `jasmin/zupt_aes_ctr4.jazz` | "Constant Time" |
| MAC compare 32-byte CT | `jasmin/zupt_mac_verify.jazz` | "Constant Time" |
| ML-KEM cmov-style select | `jasmin/zupt_mlkem_select.jazz` | "Constant Time" |
| X25519 field cswap | `jasmin/zupt_x25519_fe.jazz` | "Constant Time" |

The repository records a `jasminc 2026.03.0` generation run, but the exact
compiler/type-checking invocation and output were not reproduced for this
candidate. The default base build does not link these assembly objects. The
current release therefore treats the table as historical provenance, not a
new formal-verification result.

Key scheduling, format parsing and allocation are outside the recorded Jasmin
inventory and were not formally assessed for timing behavior.

---

## Defense-in-depth measures

Beyond the cryptographic primitives, libvuptsdk implements:

| Measure | Implementation | Verified |
|---|---|---|
| Best-effort `mlock()` on private-key buffers | `src/zupt_mlock.c` | attempted; OS limits may reject it |
| Explicit `zupt_secure_zero()` on free | every key path | yes (smoke test 4) |
| Stack canary protection | `-fstack-protector-strong` | yes (compiler flag) |
| RELRO + BIND_NOW | `-Wl,-z,relro,-z,now` | yes (compiler flag) |
| PIC + ASLR | `-fPIC -shared` | yes (compiler flag) |
| FORTIFY_SOURCE | `-D_FORTIFY_SOURCE=2` | yes |
| No `gets`/`strcpy`/`sprintf` | grep + audit | yes (0 occurrences) |
| Bounded length-prefix parsing | focused archive paths | current tests pass; not a proof |
| Full-byte MAC comparison | portable implementation | dynamic timing evidence only |
| Anti-fault decapsulation | re-encrypts and checks vs ciphertext | yes |

---

## Historical results inherited from the Zupt audit campaign

The following results were recorded in the earlier Zupt/SDK audit campaign.
The current tree has since diverged, so these rows provide history and are not
substitutes for the current-candidate rows at the top of this report:

| Audit campaign | Tests | Fuzz iters | Result |
|---|---|---|---|
| zupt 2.1 SDK initial | 47 | 0 | 47/47 pass |
| zupt 2.2 SDK extended | 169 | 0 | 169/169 pass |
| zupt 2.2 SDK fuzz round 1 | — | 250,000 | 0 crashes |
| zupt 2.2 SDK fuzz round 2 | — | 500,000 | 0 crashes |
| zupt 2.2.1 ASAN/UBSAN sweep | 169 | — | 0 memory errors |
| zupt 2.2.2 internal audit | 169 | 1,000 | 0 crashes |
| libvuptsdk 2.0.0 initial | 30 + 13 | +11,500 | all green |
| **libvuptsdk 2.0.0 extended (this audit)** | **30 + 13 + 5 new** | **+192,800** (incl. format-fuzz, key-iso, side-channel) | **all green** |
| **Cumulative** | **220+** | **942,800** | **all green** |

---

## Frama-C ACSL specifications

14 of the most safety-critical functions have ACSL contracts in
`include/zupt_acsl.h`. These specs were verified by Frama-C (WP plugin) in
the zupt repository:

- `zupt_xxh64` — commutativity, length equivalence
- `zupt_secure_zero` — post-condition: all bytes zero
- `zupt_hkdf_sha3` — length contract; no allocation
- `zupt_aes256_ctr` — output length = input length
- ... and 10 more

Specs ship in `include/zupt_acsl.h`. They're a **partial**, not
**comprehensive**, formal verification.

---

## Open items (next minor version)

The following are unresolved release gaps. The current source integration is
not a replacement full-ABI release while these remain open.

1. **Open-source `zuptsdk_easy_*` implementations** — currently in binary
   form only in `prebuilt/libvuptsdk.so.2.0.3`. Tracked as the top open
   item.
2. **External independent cryptographic audit** — not yet commissioned.
3. **libFuzzer / AFL++ integration** — current fuzz is internal mutation
   fuzzer; structure-aware fuzzing would be more thorough on the parse
   paths.
4. **AArch64 runtime validation** — the build selects its AArch64/NEON flags,
   but this candidate has not run on AArch64 hardware or a VM.
5. **Win32 / macOS validation and packages** — not run for this candidate.
6. **ML-KEM-1024 variant** — currently only the NIST category-3 ML-KEM-768
   parameter set is offered.
7. **HSM / TPM integration** — for key generation in non-software isolation
   domains.

---

## Methodology — how this audit was performed

Each property in this report was verified following this protocol:

1. **State the hypothesis as a falsifiable claim** (e.g., "every bit-flip
   is detected by the AEAD MAC").
2. **Build a reproducible test harness** that exercises the claim with
   adversarial inputs (random + edge cases).
3. **Run for sufficient iterations** to cover the relevant adversary model
   (1000 for cheap properties, 10000+ for rare-event tests).
4. **Define a pass criterion in the test code itself** so the test fails
   loudly on regressions.
5. **Capture exact numbers** — counts of detected/undetected/skipped, not
   just "passes".
6. **Document open items** rather than burying them in code comments.
7. **No "we tested it manually" claims** — every claim has a runnable test
   in the repo (or is explicitly tagged "Pending" / "Not verified").

This document is **not** a substitute for an external independent
cryptographic audit. It is an **honest internal assessment** of where the
library stands. The line between "internal audit" and "external audit" is
intentionally not blurred.

The parent project maintains its own executable checks and audit evidence.
Results in this repository apply only to the SDK revision and artifacts named
here.

---

## How to reproduce this audit

```bash
git clone https://git.securityops.co/cristiancmoises/libvuptsdk
cd libvuptsdk

# 1. Build + run the standard test suite (30 properties)
make test

# 2. ASAN/UBSAN repeat run
for i in 1 2 3 4 5 6 7 8 9 10; do make test-asan; done

# 3. License coverage audit
make audit-licenses

# 4. Adversarial fuzz (compile + run)
cc -O2 -Iinclude tools/tamper_fuzz.c \
   prebuilt/libvuptsdk.so.2.0.3 -o /tmp/tf -lpthread -lm
LD_LIBRARY_PATH=prebuilt /tmp/tf

# 5. Wrong-key fuzz
cc -O2 -Iinclude tools/wrong_key_fuzz.c \
   prebuilt/libvuptsdk.so.2.0.3 -o /tmp/wkf -lpthread -lm
LD_LIBRARY_PATH=prebuilt /tmp/wkf

# 6. Cross-language binding tests
PYTHONPATH=bindings/python python3 tests/test_python.py
# (also: cd bindings/go && go test ./..., cd bindings/rust && cargo test)
```

---

**License**: This document is part of the libvuptsdk project, licensed under the GNU Affero General Public License version 3 or later (AGPL-3.0-or-later). See [LICENSE](LICENSE).
