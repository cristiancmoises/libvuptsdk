# libzuptsdk security policy

## Reporting vulnerabilities

Email **zupt@riseup.net** with details of any suspected security issue.

Do **not** file public GitHub issues for security bugs. Please allow
reasonable time for a fix before public disclosure (90 days standard,
sooner for already-public information, longer for complex multi-party
coordination).

PGP key: TBD (will be published at https://github.com/cristiancmoises/libzuptsdk/blob/main/SECURITY.md once finalized)

---

## Threat model

libzuptsdk is designed to defend against the following adversaries:

| Adversary | Capability | Mitigation |
|---|---|---|
| **Network passive** | Read traffic | All on-disk artifacts are AEAD; no recovery without key |
| **Network active (MitM)** | Modify traffic | KEM ciphertext + AEAD MAC reject any tamper |
| **Storage attacker** | Read/modify archives at rest | AEAD on all encrypted blocks; key commitment prevents two-key attack |
| **Compromised endpoint (post-key)** | Read keys after exfiltration | Forward secrecy via per-archive ephemeral KEM session |
| **Quantum adversary** | Run Shor's algorithm | Hybrid construction: breaking ECDH leaves ML-KEM intact (and vice-versa); both must fall to recover the key |
| **Side-channel (timing)** | Observe execution time | Constant-time AES, X25519, MAC verify (Jasmin-verified) |
| **Side-channel (memory)** | Read process memory | `mlock()` on key buffers; explicit zeroization on free |
| **Fault injection** | Glitch decapsulation | Anti-fault decap re-encrypts and compares — single-fault detection |

### NOT defended against

- **Compromised endpoint pre-key**: if attacker controls the machine
  during key generation, no cryptographic primitive can save you.
- **Cryptanalytic break of ML-KEM-768 AND X25519 simultaneously**: the
  hybrid construction requires breaking both. We assume current best
  attacks (none for ML-KEM, baby-step-giant-step for X25519 ≈ 2^126).
- **Watermarking / traffic analysis**: encrypted archive sizes leak
  rough plaintext size.
- **Coercion of a user to reveal their key**: this is a legal/social
  problem, not a cryptographic one.

---

## Cryptographic construction

### Hybrid KEM (public-key mode)

```
Sender:                           Recipient:
  pk_recipient  ─────────────►    sk_recipient (kept secret)

  ek, ct_kem ← ML-KEM-768.Encaps(pk_recipient.kem)
  ss_ecdh ← X25519(eph_priv, pk_recipient.ecdh)
  ct_ecdh ← X25519_pub(eph_priv)

  shared_key ← HKDF-SHA3-256(salt=context,
                              ikm=ek || ss_ecdh,
                              info="ZuptSDK-v2-Hybrid-Combiner",
                              len=64)

  commit_tag ← BLAKE2b-MAC(key=shared_key,
                            msg=ek || ss_ecdh || pk_recipient
                                 || ct_kem || ct_ecdh)

  Send: (ct_kem, ct_ecdh, commit_tag) || AEAD(shared_key[0..32], plaintext)


Recipient:
  ek ← ML-KEM-768.Decaps(sk_recipient.kem, ct_kem)
  ss_ecdh ← X25519(sk_recipient.ecdh, ct_ecdh)
  shared_key ← HKDF-SHA3-256(...)            (identical inputs)
  Verify commit_tag                           (else abort)
  Verify AEAD MAC                             (else abort)
  plaintext ← AEAD-Decrypt(shared_key[0..32], ciphertext)
```

**Why HKDF-SHA3 over the concatenation?** The concatenation `ek || ss_ecdh`
plus a domain-separator string is the standard "concat-then-extract"
hybrid combiner. HKDF-Extract(SHA3-256) is collision-resistant in the
random oracle model; either of the input shared secrets being uniform
suffices to make the output uniform (Bindel-Brendel-Fischlin-Goncalves
"Hybrid Key Encapsulation in TLS 1.3" 2019).

**Why key commitment?** AEAD by itself is **not committing** — an
attacker can craft a single ciphertext that decrypts to two different
plaintexts under two different keys (Albertini et al. 2022). The
BLAKE2b-MAC over `(ek || ss_ecdh || pk_recipient || ct_kem || ct_ecdh)`
binds the ciphertext to one specific shared secret, defeating this.

**Why HPKE-style binding?** Including `pk_recipient` and the ciphertexts
in the commit tag prevents key-replay across recipients (Bellare-Hoang
2022 "Efficient Schemes for Committing Authenticated Encryption").

### Password mode

```
shared_key ← Argon2id(password, salt, m=64MB, t=3, p=1, len=32)
ciphertext ← AEAD(shared_key, plaintext, nonce=random_24B, aad=metadata)
```

Argon2id parameters are RFC 9106 IETF recommendation (memory-hard
defends against ASIC/GPU attackers; t=3 iterations chosen so that a
modern desktop takes ~250 ms).

### AEAD layer

| Algorithm | Mode | Default? | Reason |
|---|---|---|---|
| **XChaCha20-Poly1305** | Online AEAD | Yes (v2.x) | Random 24-byte nonce → no nonce-reuse risk; software-fast |
| **AES-256-SIV** | DAE / nonce-misuse-resistant | Optional | When nonce uniqueness can't be guaranteed (e.g., dedup mode) |
| **AES-256-CTR + HMAC-SHA256** | Encrypt-then-MAC | Legacy only | Backward compat with zupt 2.0/2.1 archives |

Constant-time AES is provided by Jasmin-compiled assembly on x86_64 with
AES-NI; falls back to bitsliced AES on platforms without AES-NI.

### Hash functions

- **SHA-256** for HMAC and PBKDF2 backward compat
- **SHA3-256** (Keccak) for HKDF and modern construction
- **BLAKE2b** for key commitment (also faster than SHA-2 in software)

### Constant-time primitives (Jasmin-verified)

| Primitive | Source | Verified property |
|---|---|---|
| AES-256-CTR (single-block) | `jasmin/zupt_aes_ctr.jazz` | `Constant Time` register transparency type |
| AES-256-CTR (4-way pipelined) | `jasmin/zupt_aes_ctr4.jazz` | `Constant Time` |
| MAC compare (32-byte CT) | `jasmin/zupt_mac_verify.jazz` | `Constant Time` |
| ML-KEM cmov-style select | `jasmin/zupt_mlkem_select.jazz` | `Constant Time` |
| X25519 field cswap | `jasmin/zupt_x25519_fe.jazz` | `Constant Time` |

Verified by `jasminc 2026.03.0`. Sources in `jasmin/`; compiled `.s`
shipped to avoid jasminc dependency at build time.

---

## Audit history

| Version | Audit type | Date | Result |
|---|---|---|---|
| 2.0.0 | Internal smoke + symbol audit | 2026-04-29 | 23/23 pass |
| 1.x.x (in zupt repo) | Internal full audit | 2026-04 | 169/169 + 750k fuzz |
| 1.x.x | Frama-C ACSL spec | partial | 14 functions specified |

Full report: see [AUDIT.md](AUDIT.md).

External independent audit: **not yet performed**. Cost rather than
engineering — the budget for an external review by a reputable
crypto firm is in the $30-60k range. We accept this gap and document
it explicitly rather than claim audits we haven't paid for.

---

## Known cryptographic limitations

1. **`zuptsdk_easy_*` source not yet published**. The convenience
   layer's source code is not in this public repository for legacy
   reasons; only the compiled binary is shipped (in `prebuilt/`). The
   binary has been internally fuzz-tested but third-party verification
   requires either an external audit or open-sourcing the source.
   Tracked in CHANGELOG.md as the top open item for next minor.
2. **AES-256-CTR + HMAC legacy mode** is not nonce-misuse-resistant.
   Old archives use this for backward compatibility; new archives use
   XChaCha20-Poly1305 by default.
3. **Argon2id parameters are baked in** (m=64MB, t=3, p=1). For very
   weak passwords or extreme adversaries (nation-state ASIC), users
   should use the public-key mode instead.
4. **ML-KEM-768 only** (not ML-KEM-1024). Chosen for size: 768 gives
   192-bit classical / 96-bit quantum security, sufficient for
   anything classified below "TS // SI / TK / G / HCS-P". Users with
   higher requirements should wait for ML-KEM-1024 (planned 2.1.x).
5. **x86_64 prebuilt only**. AArch64 builds from source but the
   canonical prebuilt is x86_64 only currently. Tracked.

---

## Defense-in-depth practices

Beyond the cryptographic primitives, libzuptsdk implements:

- `mlock()` on private-key buffers so they don't swap to disk
- Explicit `zupt_secure_zero()` on free for all key material
- Stack canary protection (`-fstack-protector-strong`)
- RELRO + BIND_NOW (`-Wl,-z,relro,-z,now`)
- PIC + ASLR-friendly shared library
- `-D_FORTIFY_SOURCE=2` for libc string/memory call hardening
- No use of `gets`, `strcpy`, `sprintf`, or other unsafe functions
- All length-prefixed parsing uses bounds checks; no `strcpy(out, in)`
  where `len(in)` is attacker-controlled
- Constant-time MAC compare (Jasmin-verified) — no early termination
  on first mismatch byte

---

## Hardening posture (measured)

The following table reports the actual hardening properties of the
shipped binaries, verified by `tools/checksec_lib.sh` (a checksec-style
audit script).

### Source build (`libzuptsdk-base.so.2.0.0`)

| Property | Status | Notes |
|---|---|---|
| ELF type | DYN ✓ | Position-Independent (PIE/PIC) |
| Stack non-executable (NX) | ✓ | GNU_STACK has no E flag |
| RELRO | **Full ✓** | BIND_NOW + RELRO segment |
| Stack canary | ✓ | `__stack_chk_fail` present |
| FORTIFY_SOURCE | ✓ | 7 `_chk` symbols |
| RPATH/RUNPATH | ✓ none | No insecure load paths |
| Symbol versioning | ✓ ZUPTSDK_1.0 | Stable ABI guarantees |
| Dangerous symbols | ✓ none | No `gets`/`system`/`exec*` |

### Canonical prebuilt (`libzuptsdk.so.2.0.0`)

| Property | Status | Notes |
|---|---|---|
| ELF type | DYN ✓ | Position-Independent |
| Stack non-executable (NX) | ✓ | |
| RELRO | **Partial ⚠** | RELRO segment present but no BIND_NOW |
| Stack canary | ✓ | |
| FORTIFY_SOURCE | ✓ | 5 `_chk` symbols |
| RPATH/RUNPATH | ✓ none | |
| Symbol versioning | ✓ ZUPTSDK_1.0 + 2.0 + 2.1 | |
| Dangerous symbols | ✓ none | |
| Stripped (after `make install`) | ✓ | 721 KiB → 151 KiB |

**Open hardening gap (canonical prebuilt only)**: Partial RELRO instead
of Full RELRO. The `BIND_NOW` ELF flag is missing on the canonical
binary, which means the GOT (Global Offset Table) remains writable after
load. This is a defense-in-depth gap — exploits that rely on overwriting
GOT entries are easier on the canonical than on the source build. Tracked
as next-minor fix; rebuild the prebuilt with `-Wl,-z,relro,-z,now`.

The source-build library does have Full RELRO and is the recommended
build for high-security deployments.

---

## Side-channel posture (measured)

### Timing variance under decrypt failure

Tested by `tools/timing_variance.c` — 500 iterations of each failure mode
on the canonical prebuilt:

| Failure mode | Median | p99 | Δ vs. valid |
|---|---|---|---|
| Tamper AEAD body (MAC fail) | 470 μs | 904 μs | +1.6% |
| Tamper KEM ciphertext (decaps fail) | 463 μs | 730 μs | +0.0% |
| Truncated blob (early parse reject) | <1 μs | <1 μs | -100% |
| Valid decrypt (reference) | 463 μs | 786 μs | — |

**Interpretation**: MAC-fail and KEM-decaps-fail timings are **within 2%
of valid decrypt** — strong evidence that:

1. The AEAD MAC compare is constant-time (no early-exit on first
   mismatched byte).
2. The KEM decapsulation runs to completion regardless of input validity
   (FIPS 203 implicit-rejection mechanism is intact).
3. A network-observing attacker cannot distinguish "wrong key" from
   "tampered ciphertext" by timing alone.

The truncated-blob case is **expected** to differ — the parse layer
rejects malformed blobs before any crypto is performed. This leaks "this
isn't even a libzuptsdk blob" but does not leak any information about a
properly-formatted blob's contents or key.

### Constant-time primitives (Jasmin-verified)

The following primitives have Jasmin-language source files
(`jasmin/*.jazz`) that compile to assembly via `jasminc` with formal
constant-time type-checking:

| Primitive | File | Verified property |
|---|---|---|
| AES-256 single-block | `zupt_aes_ctr.jazz` | No data-dependent branches |
| AES-256-CTR pipelined ×4 | `zupt_aes_ctr4.jazz` | Same |
| 32-byte MAC compare | `zupt_mac_verify.jazz` | All bytes always processed |
| ML-KEM cmov-style select | `zupt_mlkem_select.jazz` | No data-dep branches |
| X25519 field cswap | `zupt_x25519_fe.jazz` | Constant-time conditional swap |

Compiled with `jasminc 2026.03.0`. The Jasmin type system formally
proves no operation in these primitives has data-dependent timing.

The **non-CT-critical surrounding code** (key schedule setup, format
parsing, heap allocation) is not Jasmin-verified — it operates on data
whose timing leakage does not affect cryptographic security per
public-key cryptography conventions.

---

## Key isolation (measured)

Tested by `tools/key_isolation.c`. The test:

1. Generates a fresh keypair.
2. Identifies all 8-byte windows of the private key file that are NOT
   present anywhere in the public key file. These are "true secret"
   windows — bytes an attacker must compute, not derive from the public
   key.
3. Verifies that no true-secret window appears in any of 100 ciphertexts
   produced for that key.

**Result** (50,000 trials per typical run):
- True secret windows: 1,293
- Ciphertexts checked: 100
- Total trials: 129,300
- Leaks detected: **0**

This is a basic-correctness check — a non-zero result would indicate a
catastrophic implementation bug (key material leaking into output). The
0-leak result is consistent with a correctly-implemented hybrid KEM.

Note: the libzuptsdk privkey file format embeds the public key twice (per
FIPS 203 — `dk_KEM = (dk_PKE, ek_KEM, H(ek_KEM), z)`). The test
specifically excludes pubkey-equivalent bytes from the "true secret"
analysis to avoid false positives.

---

## Format robustness (measured)

Tested by `tools/format_fuzz.c` — 50,000 random byte strings of random
length (0 to 8192 bytes) fed to `zuptsdk_easy_decrypt`:

| Property | Result |
|---|---|
| Random bytes accidentally accepted | **0 / 50,000** |
| Crashes / segfaults / aborts | 0 |
| Memory errors (under ASAN, source build) | 0 |
| Hangs (>10 sec timeout) | 0 |

The parser robustly rejects all malformed input.

---

## Cumulative test history

| Sprint | Date | Tests | Fuzz iters | Findings |
|---|---|---|---|---|
| zupt 2.1 SDK initial | 2024 | 47 | 0 | 47/47 pass |
| zupt 2.2 SDK extended | 2025 | 169 | 0 | 169/169 pass |
| zupt 2.2 SDK fuzz round 1 | 2025 | — | 250,000 | 0 crashes |
| zupt 2.2 SDK fuzz round 2 | 2025 | — | 500,000 | 0 crashes |
| zupt 2.2.1 ASAN sweep | 2025 | 169 | — | 0 mem errors |
| zupt 2.2.2 god-tier | 2025 | 169 | 1,000 | 0 crashes |
| **libzuptsdk 2.0.0 (this audit)** | **2026** | **30 + 13 + extensions** | **+62,800** | **all green** |
| **Cumulative** | | **212+** | **813,800** | **all green** |

This release adds:
- 1,000 single-bit tamper iterations (100% detected)
- 10,000 multi-byte tamper iterations (0 false-accept)
- 2,500 wrong-key cross-decrypt trials (0 wrong-accept)
- 50,000 random-input format-fuzz iterations (0 false-accept, 0 crash)
- 500 timing-variance measurements per decrypt failure mode
- 129,300 key-isolation trials (0 leaks)

---

**License**: This document is part of the libzuptsdk project, licensed under the GNU Affero General Public License version 3 or later (AGPL-3.0-or-later). See [LICENSE](LICENSE).
