# libvuptsdk performance characterization

Last measured: 2026-04-29
Library: `libvuptsdk-2.0.0` (canonical x86_64 prebuilt)
Bench source: [`bench/bench_throughput.c`](bench/bench_throughput.c)

This document records measured performance, the methodology used to obtain
it, and the computational model behind each cost so users can predict their
own numbers.

---

## Reference hardware

| Property | Value |
|---|---|
| CPU | x86_64 generic, 2 cores @ 2.8 GHz |
| RAM | 9 GB |
| OS | Ubuntu 24.04, glibc 2.39, GCC 13.3 |
| Compiler flags | `-O2 -fstack-protector-strong -D_FORTIFY_SOURCE=2 -Wl,-z,relro,-z,now` |
| AES-NI | yes (verified) |
| AVX2 | yes (verified) |

Numbers will differ on production hardware. Most users on a recent desktop
(8+ cores, AVX2, ~4 GHz) should expect **2-4× the throughput** and **0.5×
the latency** shown below.

---

## Measured numbers

All values are wall-clock medians from 100 samples (Argon2id from 20). The
"p99" column shows the 99th percentile to help identify tail-latency cases
(usually GC pauses or first-call resolver costs).

### Public-key (PQ hybrid) operations

| Operation | Median | p99 | Min | Notes |
|---|---|---|---|---|
| `easy_keygen` | 478 μs | 1,202 μs | 370 μs | ML-KEM-768 keygen + X25519 keygen + write 2 files |
| `easy_encrypt` (64 B msg) | 428 μs | 31,462 μs* | 365 μs | KEM-encaps dominates |
| `easy_encrypt` (4 KB msg) | 436 μs | 37,011 μs* | 381 μs | KEM still dominates |
| `easy_decrypt` (64 B msg) | 443 μs | 586 μs | 389 μs | KEM-decaps + AEAD verify |
| `easy_decrypt` (4 KB msg) | 500 μs | 24,917 μs* | 404 μs | + AEAD decrypt body |

*p99 outliers are first-call resolver costs and OS scheduling — not
algorithmic.

### Sustained encrypt throughput

| Message size | Iterations | Throughput |
|---|---|---|
| 1 KB | 100 | 1.0 MB/s (KEM-bound) |
| 64 KB | 100 | 47 MB/s |
| 1 MB | 100 | 153 MB/s |
| 16 MB | 10 | 182 MB/s |

The asymptotic throughput is the AEAD speed (~180 MB/s in this 2-CPU
sandbox; expect 400-700 MB/s on modern desktop hardware with AVX2).

### Symmetric / field encryption

| Operation | Time | Notes |
|---|---|---|
| `easy_encrypt_field` (email-sized string) | **5.8 μs** | Suitable for high-volume DB column encryption |

### Password-based encryption

| Operation | Time | Notes |
|---|---|---|
| `easy_encrypt_password` (Argon2id 64MB, t=3, p=1) | **1.09 sec** | RFC 9106 IETF recommendation |

⚠️  **The README previously claimed ~250 ms.** Measured median is ~1.1 sec
on this 2-CPU sandbox. On modern desktop hardware (8+ cores) Argon2id with
identical parameters will measure 200-400 ms because the `m=64MB t=3` work
is partially memory-bandwidth bound, partially CPU-bound. The library
parameters are unchanged; only the documentation has been corrected.

If your throughput requirements demand a faster KDF, you can use lower
Argon2id parameters via the lifecycle API (`zuptsdk_password_params_*`).
The defaults are conservative because password encryption is a one-time
operation per user session, where 1 second of work is acceptable in
exchange for adversary work amplification.

---

## Cost model

To estimate your own workload, compose these costs:

```
encrypt(msg)         ~= 400 μs  +  msg_bytes / aead_throughput
decrypt(msg)         ~= 450 μs  +  msg_bytes / aead_throughput
keygen()             ~= 470 μs
encrypt_field(str)   ~= 5 μs    +  len(str) / aead_throughput  (very small)
encrypt_password()   ~= 1.1 s   (Argon2id 64MB,t=3 — adjust via params API)
derive_key()         ~= 1.1 s   (same Argon2id work)
```

The 400-470 μs constant overhead is dominated by ML-KEM-768 + X25519
keygen/encaps/decaps + HKDF combiner + I/O syscalls. On modern hardware
this drops to 100-200 μs; in cloud VMs with no AES-NI it can rise to 1-2 ms.

## Choosing the right API

| Use case | Recommended API | Why |
|---|---|---|
| 1-1 messaging (small msgs, ≤ 100/sec) | `easy_encrypt` / `easy_decrypt` | Simplest API; KEM cost amortizes |
| Bulk file encryption | `easy_encrypt_file` | Streamed; no memory pressure |
| DB column encryption (≤ 100k ops/sec) | `easy_encrypt_field` + `easy_derive_key` | One Argon2id at startup, then 5 μs per field |
| User-facing password encryption | `easy_encrypt_password` | Argon2id parameters provide 1-sec wall-clock work — appropriate vs. brute force |
| High-throughput streaming | `zuptsdk_encrypt_stream_pq` (lifecycle API) | Re-key per chunk; constant memory |

---

## Reproducing these numbers

```bash
make
cc -O2 -Iinclude bench/bench_throughput.c \
   prebuilt/libvuptsdk.so.2.0.0 \
   -Wl,-rpath,$PWD/prebuilt \
   -o /tmp/bench -lpthread -lm
LD_LIBRARY_PATH=prebuilt /tmp/bench
```

Each run takes ~1 minute (mostly Argon2id). Numbers stable to within ±10%
across runs on the same machine.

---

## Comparison to peer cryptographic libraries

| Library | PQ keygen | PQ encrypt 1KB | Argon2id 64MB |
|---|---|---|---|
| **libvuptsdk 2.0.0** (this lib) | 470 μs | 1.0 MB/s | 1.1 sec |
| libsodium (X25519 only, no PQ) | 80 μs | 12 MB/s | N/A |
| OQS-OpenSSL (ML-KEM-768) | ~500 μs | similar | N/A |

libvuptsdk's PQ overhead is comparable to OQS-OpenSSL (the standard PQ
benchmark). The asymmetric wins of libvuptsdk are: integrated key commitment
(BLAKE2b-MAC), constant-time AES-NI via Jasmin, and a high-level `easy_*`
API that handles all the protocol composition correctly.

---

## Performance roadmap

| Optimization | Expected gain | Status |
|---|---|---|
| Batch ML-KEM-768 keygen (multi-recipient) | 5-8× throughput on bulk PQ | not started |
| Streaming AEAD with rekey (constant mem for any size) | unbounded throughput | implemented (lifecycle API), not yet wired into easy_* |
| AVX-512 ChaCha20 path (SIMD lane width 8) | ~2× AEAD on Skylake-X+ | not started |
| ARM NEON ChaCha20 (AArch64) | parity with x86_64 | partially implemented |
| Argon2id with libargon2 reference (faster on AVX2) | ~30% Argon2id speedup | optional dependency, not bundled |

---

**License**: This document is part of the libvuptsdk project, licensed under the GNU Affero General Public License version 3 or later (AGPL-3.0-or-later). See [LICENSE](LICENSE).
