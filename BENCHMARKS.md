# libvuptsdk performance measurements

The measurements below belong to 2.1.0-base.1. They were not rerun for
2.1.0-base.2 and do not establish the new release's throughput.

Release: **2.1.0-base.1**. Updated: **2026-09-24**.

The source API and a separate Bend model were measured on an **Intel Core
i7-13700HX**, with 16 cores and 24 logical CPUs. These are local measurements
for the specified inputs. They do not establish a general throughput guarantee
or infer performance from a proof.

## Current source release: SDK before and after

The same public archive API workload was run against **2.0.4-base.1** and
**2.1.0-base.1**. Both builds used GCC 14.3.0, `-O2`, baseline x86-64 SSE2 and
no final linker RPATH, on Linux x86-64 with glibc 2.41.

The workload uses deterministic records, explicitly selects VaptVupt, and sets
one SDK thread. Each size/level combination has one warm-up and five measured
iterations. Values below are medians. Compression and extraction include SDK
temporary-file work; they are not isolated codec throughput or encrypted-mode
benchmarks.

| Input | Level | Archive bytes, before → after | Compress ms, before → after | Extract ms, before → after |
|---|---:|---:|---:|---:|
| 64 KiB | 1 | 10,868 → 10,900 | 0.2611 → 0.2750 | 0.0647 → 0.0733 |
| 64 KiB | 9 | 3,778 → 3,820 | 12.5702 → 14.1959 | 0.1188 → 0.1197 |
| 1 MiB | 1 | 87,768 → 87,800 | 1.3669 → 1.5262 | 0.4569 → 0.5313 |
| 1 MiB | 9 | 7,452 → 3,880 | 28.7571 → 20.9128 | 0.5728 → 0.7447 |

The 1 MiB level-9 case compresses faster and smaller on this corpus. Other
compression cases and all extraction medians are higher in this run; five
samples do not establish whether small timing differences are significant.
Format 1.6 adds a 32-byte AIT; the refreshed wrapper also verifies candidate
compressed frames by decoding them. Level-9 block sizing changes how this
particular corpus is compressed. The measurements do not isolate each change's
individual cost or support a universal speedup claim.

The [recorded results](bench/archive-results.json) contain the aggregate
medians and archive sizes. To measure the current build:

```sh
make -j4
python3 bench/bench_archive.py build/libvuptsdk-base.so --iterations 5
```

To repeat the comparison, build the earlier `2.0.4-base.1` source revision in a
separate directory and run the same script against its library path. The
baseline binary is not shipped with this release. Retain the printed JSON and
record your compiler and machine when comparing runs.

## Bend model: measured native CPU comparison

Measured on 2026-09-24 with **Bend 2.0.5**, **Clang 22.1.8**, Linux x86-64,
glibc 2.41 on the host identified above, and CPU execution only. The same
native binary was run with 1 and 24 threads, with one warm-up and seven
measured samples per configuration.

The workload generates a binary tree with 2²⁰ leaves, assigns each leaf a byte
count of `(index % 256) + 1`, and reduces its count and sum. Both configurations
returned **1,048,576 leaves** and **134,742,016 bytes**.

| Native CPU configuration | Median wall time |
|---|---:|
| 1 thread | 0.021306821 s |
| 24 threads | 0.006099247 s |
| Ratio of medians, 1 thread / 24 threads | 3.49× |

Timing includes tree generation, reduction, process startup and output. This
comparison measures the pure Bend model; it does not measure a speedup of the
C SDK, codec or cryptography. No Bend subprocess is added to an SDK operation,
and no GPU benchmark is claimed.

Reproduce the law check and native benchmark from the repository root:

```sh
bend PROOF.bend
python3 formal/benchmark.py --output formal/benchmark-results.json
```

The [raw results](formal/benchmark-results.json) record all samples, toolchain
versions and source hashes. The benchmark runner builds a native executable;
timing the default JavaScript runner would measure a different backend. The
10 laws in [LAWS.bend](LAWS.bend) concern the modeled tree operations and their
assumptions, not an independently implemented C kernel or an operating system.

## Historical full-ABI measurements

The previous report recorded the following on **2026-04-29**, using the
historical full-ABI **2.0.0** binary on a 2-core x86-64 environment at 2.8 GHz,
Ubuntu 24.04, GCC 13.3 and `-O2`:

| Operation | Recorded result |
|---|---:|
| `easy_keygen` | 478 µs median |
| `easy_encrypt`, 64 bytes | 428 µs median |
| `easy_decrypt`, 64 bytes | 443 µs median |
| `easy_encrypt_field` | 5.8 µs median |
| `easy_encrypt_password`, Argon2id 64 MiB, t=3, p=1 | 1.09 s median |
| Sustained encryption, 16 MiB messages | 182 MB/s |

These records were not rerun for 2.1.0-base.1, the 2.0.0 binary is not part of
this release, and the base API does not provide those `easy_*` operations.
They are retained only to identify the scope of historical performance claims.
The old [benchmark source](bench/bench_throughput.c) targets the historical full
ABI. No expected speedup on newer CPUs or comparison against other libraries is
inferred from these numbers.

Copyright 2026 Cristian Cezar Moisés. [Apache-2.0](LICENSE).
