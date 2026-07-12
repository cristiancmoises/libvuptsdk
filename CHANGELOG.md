# libvuptsdk changelog

All notable changes to libvuptsdk are documented in this file. The
format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
at the ABI level (see README.md "Versioning").

---

## [2.0.2] — 2026-07-12

### Changed

- **Synced the embedded VaptVupt codec to upstream v2.48.2**
  (`git.securityops.co/cristiancmoises/vaptvupt-codec`). The core codec files
  (`src/vv_ans.c`, `vv_decoder.c`, `vv_encoder.c`, `vv_huffman.c`, `vv_simd.c`,
  `vv_xxh64.c` and the `vv_*`/`vaptvupt.h` headers) are now **verbatim** copies
  of v2.48.2, differing only in the SPDX tag (AGPL, per this repo's license
  gate). Brings the Sprint 118 encoder buffer scrubbing (`vv_secure_zero`
  before `free`), Sprint 117/118 hardened-build (`-fsanitize=integer`)
  cleanliness, and the Sprint 120/121 cost-aware lazy-parser ratio gains
  (aggregate now −1.07% vs zstd-3). Upstream's `tests/test_zupt_integration.c`
  (TEST19, 9 cases) passes against the synced tree.

### Fixed

- **VaptVupt fast-level (≤2) archives were undecodable (data loss).** The
  Zupt wrapper set `format_v2 = 1` unconditionally, but `format_v2` + the
  `ULTRA_FAST` mode used for levels ≤2 produces a frame that fails to decode
  with `VV_ERR_OVERFLOW` in every codec version. `vvz_compress` now sets
  `format_v2 = 0` (also the safest choice for cross-decoder compatibility —
  see caveat below). Verified: all six representative payload classes now
  round-trip at levels 1/5/9 (18/18).

### Integration notes

- `vvz_compress` follows the v2.48.2 `ZUPT_INTEGRATION.md` guidance:
  `checksum = 0` on encode (Zupt's outer AEAD/HMAC authenticates the bytes;
  the decode path already passed `VV_DECOMPRESS_SKIP_CHECKSUM`), and
  `compat_v246_5_decoder = 1` to suppress the newest literal format (lit_fmt=4)
  for broader decoder-version compatibility.

### Known limitation (prebuilt archive format)

- The v2.48.2 encoder emits modern Huffman literal formats (**lit_fmt=3/4**)
  that the canonical prebuilt's **older embedded decoder predates and cannot
  read**; no encoder option can suppress lit_fmt=3. Consequently an archive
  compressed with the VaptVupt codec by the **from-source** library may not
  extract via the **prebuilt** for text at BALANCED/EXTREME levels. Self-
  roundtrip within either library is unaffected. Full source↔prebuilt archive
  interop requires regenerating the prebuilt from v2.48.2 — the same
  regenerate-the-prebuilt action already noted for the ML-KEM fix (2.0.1).

### Upstream finding (reported, not forked)

- Decoding a crafted corrupt **huf4** frame (`lit_fmt=4` inflate) triggers
  UBSan shift-exponent errors in `vv_ans.c` — reproduced **identically in
  pristine upstream v2.48.2**, so it is an upstream issue, not a sync
  regression. The decoder still returns a clean `VV_ERR_CORRUPT` with no
  memory-safety error (ASan clean). To be fixed upstream in vaptvupt-codec;
  the SDK mirrors upstream verbatim rather than forking the codec.

### Audit fixes (full `audit-all` sweep, 2026-07-12)

- **`make audit-licenses` was failing**: the 8 conformance-suite sources/
  scripts added in 2.0.1 lacked the AGPL SPDX header the gate requires. Headers
  added; gate back to zero missing.
- **Vacuous-pass bug in `tests/run_audit.sh`**: the architecture check shelled
  out to `file(1)`; on systems without it, both probes returned empty strings,
  and empty == empty **passed** the check ("Architecture matches ()"). Now uses
  `readelf -h` (present wherever `nm`/`readelf` already are) and requires a
  non-empty match, so a failed probe can no longer masquerade as a pass.
- **Same class of bug in `tools/checksec_lib.sh`**: the stripped-binary check
  used `file(1)` and silently reported "✓ stripped" when `file` was absent.
  Now section-based (`readelf -S` for `.symtab`/`.debug_info`). This exposed
  that the canonical prebuilt actually ships **full DWARF debug info and a
  symbol table** (previously mis-reported as stripped) — informational WARN;
  `make install` already strips at install time.
- Re-verified after fixes: license 0 missing; symbol audit 13/13 (arch now
  genuinely compared); checksec 0 failures on both libraries; adversarial fuzz
  suite all green — tamper 1000/1000 detected, multi-tamper 0 undetected in
  10,000, wrong-key 2450/2450 rejected, format fuzz 0 accepts / 0 crashes in
  50,000, key isolation 0 leaks in 129,300 trials, timing variance MAC-fail vs
  valid-decrypt medians within 0.2%.

---

## [2.0.1] — 2026-07-04

Security-correctness release for the ML-KEM-768 layer. No public ABI change
(`ZUPTSDK_1.0` / `ZUPTSDK_2.1` symbol sets are untouched); the new key-check
functions are internal (hidden by the version script). Applies to the
**from-source** library — see the prebuilt caveat below and in SECURITY.md.

### Fixed

- **security(mlkem): ML-KEM-768 now conforms to FIPS 203 and interoperates.**
  Two defects made the prior implementation self-consistent but non-standard
  (0/60 official NIST ACVP vectors; could not interoperate with any conforming
  implementation):
  1. The K-PKE matrix was generated transposed — index bytes appended as
     `(i,j)` instead of the FIPS 203 `(j,i)` for `Â[i][j]` (and the matching
     `A^T` convention in encrypt). Fixed at both sampling sites.
  2. The shared secret used the pre-final Kyber round-3 KDF
     `SHAKE256(K ‖ H(c))` instead of the FIPS 203 final construction: `K` from
     `G(m ‖ H(ek))` used directly, with implicit-rejection value `J(z ‖ c)`
     over the full ciphertext.
  Post-fix: **60/60** core ACVP KATs; byte-for-byte differential agreement with
  kyber-py 1.2.0 and RustCrypto `ml-kem` 0.2.3 in both directions; 1000/1000
  self-roundtrips; ASan/UBSan clean. No secret-dependent branch/index
  introduced; the constant-time `cmov` selection is preserved. Full root-cause
  analysis in `MLKEM_CONFORMANCE_FIX.md`.
- **docs(security): corrected the false "constant-time T-table" claim on the
  pure-C AES-256.** `src/zupt_aes256.c` uses a byte-indexed S-box (a cache-timing
  side channel); the constant-time AES is the Jasmin AES-NI backend built with
  `-DZUPT_USE_JASMIN`. Scope now documented accurately in the source header and
  SECURITY.md.

### Added

- **FIPS 203 §7.2 / §7.3 input validation.** `zupt_mlkem768_encaps` now performs
  the mandated encapsulation-key modulus check and rejects malformed keys;
  `zupt_mlkem768_check_ek` / `zupt_mlkem768_check_dk` expose the §7.2 / §7.3
  checks. This lifts ACVP coverage to the full **80/80** (adds the 10
  encapsulationKeyCheck + 10 decapsulationKeyCheck vectors that were previously
  skipped). Valid keys are unaffected (0 valid keys rejected across 1000 trials).
- **`conformance-suite/`** — a blocking ACVP-KAT + differential + constant-time
  CI gate (`.forgejo/workflows/mlkem-conformance.yaml`) so this defect class
  cannot recur. Acceptance: ACVP 80/80 **and** both differentials byte-equal in
  both directions **and** the dudect/ctgrind gate clean.

### Hardened

- **Secret wiping** of ML-KEM stack temporaries that previously survived: the
  `d ‖ k` seed in K-PKE keygen, and the message/noise polynomials (`m`, `w`,
  `u`, `v`) in K-PKE encrypt/decrypt. One-shot SHA3/SHAKE now wipe the sponge
  state after squeezing.
- **Conformance-suite robustness** (it is a blocking gate, so a false pass is a
  security bug): `run_kats.py` now fails loudly if the executed vector count is
  not 80 (empty/truncated files can no longer report "0/0 → pass"); the KAT
  driver validates argument count and exits non-zero on an unknown mode; the CT
  runner requires proof valgrind actually executed (no silent pass when valgrind
  is absent or the harness crashes) and separates a definite dudect leak
  (blocking) from statistical WARN-zone noise (advisory).

### Known limitation (prebuilt)

- The canonical `prebuilt/libvuptsdk.so.2.0.0` is **not** rebuilt from this
  patched source and must be regenerated + re-audited before it can be claimed
  FIPS 203 conformant. Archives/keys created by a pre-fix build do **not**
  interoperate with a post-fix build (the corrected KEM derives a different
  shared secret → AEAD MAC fails). See SECURITY.md limitation 6.

---

## [2.0.0] — 2026-04-29

### Extended verification sprint (audit-extended; v2.0.0 final)

After the initial 16-phase audit, an extended sprint added 5 new test
categories raising the verification surface to **22 phases** and the
cumulative fuzz iteration count to **942,800+**. All green.

**New tests** (all shipped in `tools/` for reproducibility):

1. **Side-channel timing variance** (`tools/timing_variance.c`) — 500
   iterations per failure mode. MAC-fail and KEM-decaps-fail medians are
   within **1.6%** of valid-decrypt timing. No information leakage by
   timing channel.
2. **Format fuzz** (`tools/format_fuzz.c`) — 50,000 random byte strings
   fed to decrypt. **0/50,000 false-accepts, 0 crashes.**
3. **Key isolation** (`tools/key_isolation.c`) — 1,293 distinct secret
   windows checked across 100 ciphertexts (129,300 trials).
   **0 secret-key bytes leak into ciphertext.**
4. **Hardening audit** (`tools/checksec_lib.sh`) — comprehensive ELF
   inspection (RELRO, NX, stack canary, FORTIFY_SOURCE, RPATH,
   symbol versions, dangerous symbols, strip status).
5. **Performance benchmarks** (`bench/bench_throughput.c`) — wall-clock
   median + p99 latency and sustained throughput. **First measured-and-
   reproducible perf characterization for libvuptsdk.**

**New documentation**:

- `BENCHMARKS.md` (159 lines) — full performance characterization with
  cost model and reproduction instructions
- `doc/API_REFERENCE.md` (403 lines) — every public function with full
  signature, parameters, returns, examples, threading, ownership rules
- `doc/TROUBLESHOOTING.md` (238 lines) — common build/runtime issues
- `SECURITY.md` extended with hardening posture, side-channel posture,
  key-isolation results, format-fuzz results, cumulative test history
- `AUDIT.md` extended with all 5 new tests and revised TL;DR table
  (22 verification phases)

**New Makefile targets**:

```
make audit-hardening   # ELF property audit
make audit-fuzz        # All 6 fuzzers (tamper, multi-tamper, wrong-key,
                       #   format, key-isolation, timing-variance)
make audit-all         # Full sweep: smoke + ASAN + license + hardening + fuzz
make bench             # Performance benchmark
```

**Findings & corrections from this sprint**:

- **Documentation correction**: README previously claimed Argon2id at
  ~250 ms. Measured median is ~1.09 sec on 2-CPU/2.8 GHz. Corrected to
  honest numbers in BENCHMARKS.md and README.md. (Hardware varies; modern
  desktop with 8+ cores will be 200-400 ms.)
- **Hardening gap (canonical prebuilt)**: Partial RELRO instead of Full
  RELRO — `BIND_NOW` ELF flag missing. Source build has full hardening.
  Tracked as next-minor fix; recommendation is to rebuild the prebuilt
  binary with `-Wl,-z,relro,-z,now`.
- **Install hygiene**: `make install` now strips debug info from the
  installed library (721 KiB → 151 KiB, 5× smaller). Override with
  `make install STRIP_INSTALL=0` for debug installs.

### Repository established

libvuptsdk has been split out of the monolithic `zupt` repository into
a standalone library project. This change improves:

- **Audit hygiene**: the SDK now has its own scoped CHANGELOG and AUDIT
  files. Downstream library users can answer "what's been verified in
  libvuptsdk specifically?" without wading through CLI-side history.
- **Release cadence**: SDK versions can ship independently. CLI bug
  fixes no longer drag the SDK along, and SDK feature additions don't
  block CLI patch releases.
- **Build hygiene**: SDK CI is decoupled from the CLI's jasminc +
  AArch64 QEMU pipeline. Per-change CI cost halved.

### License unification — IMPORTANT

The five Jasmin assembly source files (`jasmin/*.jazz`) previously
declared "MIT License" in their headers as a copy-paste artifact from
an earlier draft. They have been **relicensed to AGPL-3.0-or-later**
to match the rest of the project. Cristian Cezar Moisés is the sole
author of all five files, so this relicensing is the author's own
prerogative. No external contributor's work was relicensed.

The five compiled assembly files (`jasmin/*.s`) are generated output
from the corresponding `.jazz` sources via `jasminc 2026.03.0`; they
inherit the AGPL-3.0-or-later license of their inputs.

Additionally, four VaptVupt files (`src/vaptvupt_api.c`,
`include/vaptvupt.h`, `include/vaptvupt_api.h`, `include/vv_platform.h`)
declared "GPL-3.0-or-later" instead of "AGPL-3.0-or-later". These have
been corrected to AGPL-3.0-or-later. VaptVupt is also Cristian Cezar
Moisés's own work (sole author of the LZ + tANS codec), so this is
also a sole-author relicensing.

After these changes, every file in the repository carries an explicit
`SPDX-License-Identifier: AGPL-3.0-or-later` header. The new
`make audit-licenses` target verifies this on every build/CI run.

### Hard verification audit (10 phases, all green)

A 10-phase line-by-line verification was performed prior to first
release. Findings and fixes:

1. **License headers** — every `.c`, `.h`, `.hpp`, `.py`, `.sh`,
   `.yml`, `.jazz`, `.s`, `Makefile`, and `.map` file now has SPDX +
   copyright. Five `.jazz` files relicensed from MIT (see above).
   Markdown docs (`AUDIT.md`, `CHANGELOG.md`, `SECURITY.md`) gained
   license footers. `README.md` already had one.
2. **Compiler warnings** — GCC `-Wall -Wextra -Wpedantic
   -Wmissing-prototypes -Wstrict-prototypes`: zero warnings on the
   from-source library and on `tests/smoke_test.c`. Fixed:
   `zupt_mlkem768_selftest` was missing prototype (made `static
   __attribute__((unused))`); `mkstemp` warning in smoke_test
   (added `_DEFAULT_SOURCE`/`_XOPEN_SOURCE` defines).
3. **Symbol leakage** — source-built `libvuptsdk-base.so` exports 55
   symbols, all `zuptsdk_*` namespaced, zero internal leakage (no
   `zupt_*`, `vv_*`, or `decompress_*` exported). Verified by `nm -D`.
4. **C++ ABI compatibility** — all 9 public-API headers
   (`zuptsdk.h`, `zuptsdk_easy.h`, `zuptsdk_metrics.h`, 6×
   `zsdk_*.h`) gained `#ifdef __cplusplus / extern "C"` guards.
   Verified by compiling and linking a C++17 program that includes
   all 9 headers.
5. **Memory safety** — ASAN/UBSAN with `detect_leaks=1`: zero leaks,
   zero memory errors. New `tests/source_smoke.c` exercises the
   source-buildable subset under sanitizer (6/6 properties).
6. **Heap balance** — 159 `malloc`/`calloc`/`realloc` sites, 321
   `free()` sites (multiple cleanup paths share frees, normal). ASAN
   confirms no leaks.
7. **`make install` end-to-end** — `pkg-config` file is now
   regenerated at install time using current `$(PREFIX)`,
   `$(LIBDIR)`, `$(INCLUDEDIR)`. Verified: `make install
   PREFIX=/opt/zupt` produces a `.pc` with `/opt/zupt` paths, and
   `pkg-config --cflags --libs vuptsdk` reports them correctly.
   Previously: hardcoded `/usr/local`.
8. **Cross-compile arch detection** — `$(CC) -dumpmachine` correctly
   selects NEON SIMD flags when `CC=aarch64-linux-gnu-gcc` is passed.
   Verified with a fake-dumpmachine wrapper.
9. **Hermetic dist tarball** — `make dist` produces a 91-file tarball
   with no `.o`, no `__pycache__`, no `build/` directory. Fresh
   extract followed by `make` + `make test` + `make test-asan`
   succeeds with 29/29 properties green.
10. **Documentation** — README/CHANGELOG/SECURITY/AUDIT all reflect
    the current state. No stale references to the zupt monorepo
    SDK paths.

### Added

- `Makefile` with two-library build (source + canonical prebuilt) and
  `make audit` target that verifies source build is a strict subset of
  canonical
- `tests/smoke_test.c` — 10 properties exercising the documented public
  API only (no internal symbols, no test fixtures)
- `tests/source_smoke.c` — 6 ASAN/UBSAN-clean properties for the
  source-buildable subset (used by `make test-asan`)
- `tests/run_audit.sh` — symbol-level audit (architecture, SONAME, ABI
  versions, common-symbol overlap, namespace hygiene, no leakage)
- `src/zupt_sdk_stubs.c` — weak stubs for `zupt_sdk_*` functions whose
  real implementations live in the zupt CLI tree (allows source build
  to link cleanly)
- `prebuilt/libvuptsdk.so.2.0.0` — bundled canonical binary with full
  ZUPTSDK_1.0 + ZUPTSDK_2.1 ABI including the `easy_*` layer
- pkg-config support: install ships `vuptsdk.pc` with install-time PREFIX
- Symbol versioning via `zuptsdk.map`: source build emits ZUPTSDK_1.0
  symbols only; canonical adds ZUPTSDK_2.1
- C++ guards on all public headers (`extern "C"` blocks)
- `make dist` target for source tarball generation

### Inherited from zupt repository (preserved)

- Full ZUPTSDK_1.0 + ZUPTSDK_2.1 ABI (68 exported symbols in canonical)
- ML-KEM-768 + X25519 hybrid KEM with HKDF-SHA3 combiner
- XChaCha20-Poly1305 / AES-256-SIV / AES-256-CTR+HMAC AEAD options
- Argon2id password KDF
- Streaming AEAD for large objects
- BLAKE2b-MAC key commitment
- HPKE-style context binding
- Anti-fault decapsulation hardening
- Constant-time x86_64 primitives via Jasmin (AES-NI, X25519, MAC compare)
- 169 unit tests + 750k cumulative fuzz iterations under ASAN/UBSAN
- Python ctypes bindings

### Changed (vs. zupt-monorepo SDK builds)

- SONAME bumped from `libvuptsdk.so.1` (zupt 2.1.x) to `libvuptsdk.so.2`
  (this release). Already shipped in zupt 2.2.0+; the bump reflects the
  ZUPTSDK_2.1 ABI extension (easy_* layer added)
- `zupt_crypto_sdk.c` removed from libvuptsdk source list — that file is
  CLI glue and lives in the zupt repo, not here
- Source build excludes CLI-only entry points (no `zupt_main.c`)
- Internal symbol leakage closed via `--version-script=zuptsdk.map`
  (was: 128 internal `zupt_*`/`vv_*` symbols leaked from monorepo build)
- `pkg-config` file now regenerated at install time so `PREFIX=` overrides
  produce correct `.pc` content (previously: hardcoded `/usr/local`)
- All `.jazz` Jasmin source files relicensed from MIT to AGPL-3.0-or-later
  (sole-author relicensing; see "License unification" above)

### Fixed

- Stale x86_64 `.o` artifacts no longer ship in tarballs (the
  `find -delete` rule in clean now respects `prebuilt/`)
- Build directory race condition during parallel make resolved by
  ordering all build artifacts under a single `$(BUILD_DIR)` target
- pkg-config `prefix=` was hardcoded at build time instead of install
  time — fixed; install now regenerates the .pc using current PREFIX

### Open items (not blocking release, tracked for next minor)

- Open-source `zuptsdk_easy_*` implementations so the from-source
  library can become a true drop-in replacement for the prebuilt
- External independent cryptographic audit (cost, not engineering)
- libFuzzer / AFL++ infrastructure for continuous fuzzing
- ARM64 prebuilt binary (currently x86_64 only — source build works
  on both, but the canonical prebuilt is x86_64 only)

---

## ABI compatibility commitment

libvuptsdk follows strict ABI versioning. The contract:

- **No symbol in `ZUPTSDK_1.0` will ever be removed or change behavior.**
- **No symbol in `ZUPTSDK_1.0` will change its function signature.**
- **New symbols added in 2.x will go into `ZUPTSDK_2.x` blocks** in the
  version script. Old code linked against `ZUPTSDK_1.0` keeps working.
- **The C++ header `zuptsdk.hpp` is a thin RAII wrapper** over the C
  ABI. C ABI compatibility is what matters; the header may evolve.
- Any incompatible ABI change is a `libvuptsdk.so.3` event (major
  SONAME bump, separate parallel-installable library).

---

## Pre-2.0.0 history

Pre-2.0.0 versions of libvuptsdk shipped as part of the monolithic zupt
repository (`zupt/sdk/`). Their changelog history lives in zupt's
CHANGELOG.md prior to the v2.2.2 god-tier audit. Notable pre-split
milestones:

- **1.0.0** (zupt 2.1.0): first stable ABI; ML-KEM-768 + X25519 + HKDF
- **1.0.1** (zupt 2.1.5): Argon2id parameters tuned per RFC 9106 IETF
  recommendation
- **1.1.0** (zupt 2.1.7): BLAKE2b key commitment + HPKE binding added
- **2.0.0-pre** (zupt 2.2.0): `easy_*` layer added; SONAME bumped
- **2.0.0** (this release): split out as standalone repo

---

**License**: This document is part of the libvuptsdk project, licensed under the GNU Affero General Public License version 3 or later (AGPL-3.0-or-later). See [LICENSE](LICENSE).
