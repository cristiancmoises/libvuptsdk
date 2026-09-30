# libvuptsdk release verification

## 2.1.0-base.2 candidate checks — 2026-09-30

The current candidate integrates Zupt 5.2.10 and VaptVupt 2.65.13 while
preserving the SDK adaptations recorded in UPSTREAM.json. Fresh local checks
passed the 12 source smoke cases, 57 codec integration cases, 74 engine cases,
the public example, three release-package cases, and the license inventory.
The separately built Zupt 5.2.10 CLI passed all 10 archive interoperability
directions. `bend PROOF.bend` checked the existing 10 model laws; these laws
do not prove the C implementation or operating system.

Fresh `make test-asan` passed the source, codec and engine checks under
ASan/UBSan. The canonical allocator-limit harness compiled against the SDK
codec passed all 432 cases, including NULL empty inputs. The 60 exported
name/type/version entries match the previous base.1 library. All 42 upstream
reference hashes and both immutable source commits are recorded and checked.

Unsigned runtime/development DEBs and RPMs, plus an SRPM, were built from the
current source. Guix-generated library RUNPATH was removed explicitly with
patchelf; the RPM build also used scoped standard `/usr` and make-path macros
without changing installed system macros. Both extracted development
examples linked and round-tripped byte-exact, with no RPATH/RUNPATH in the
packaged libraries. This is payload validation on glibc 2.41, not a
Debian/Ubuntu/Fedora installation test. Signed artifact checks and exact
tagged-tree source-package verification remain release-publication gates.
Docker could not start its configured runc runtime, and Podman rejected its
image under the host policy; neither attempt establishes a distro install
test. Earlier results below are historical and are not transferred to base.2.

Release: **2.1.0-base.1**. Updated: **2026-09-24**.

This report applies to the source-built base API integrating Zupt 5.2.9 and
VaptVupt codec 2.65.11. The frozen full-ABI `libvuptsdk.so.2.0.3` is excluded
from new packages and does not inherit the results of source-build checks.
Internal verification is not an independent external cryptographic audit.

## Historical 2.1.0-base.1 release evidence

The source and engine checks below were rerun after the import. Final artifact
delivery and signature verification are recorded with the release; follow the
[README verification procedure](README.md#download-verify-and-extract) for
downloaded files.

| Check | Command or evidence | Current result |
|---|---|---|
| Source API, codec and engine | Source, codec and engine test binaries | 12 + 57 + 74 checks passed |
| ASan and UBSan | `make test-asan` | Source, codec and engine checks passed |
| Public example | `doc/example.c`, included by `make test-source` | Archive round trip passed |
| Exported ABI | Dynamic-symbol comparison with the previous base ABI | Only the new setter under `ZUPTSDK_1.2` added |
| Independent Zupt interoperability | Installed Zupt 5.2.9, both read/write directions | 10 directions passed |
| License inventory | `make audit-licenses` | 121 source files, zero errors |
| ML-KEM ACVP | [Conformance suite](conformance-suite/README.md) | 80/80 vectors passed |
| ML-KEM differential | kyber-py and RustCrypto `ml-kem` 0.2.3 | Each passed 100/100 in both directions |
| ML-KEM timing diagnostics | GCC `-O2` harness, 60,000 measurements per experiment | max \|t\| 1.515 accept/reject; 2.038 fixed/random accept |
| ML-KEM taint diagnostics | Valgrind 3.27.0 harness | No conditional jump/move dependent on tainted data reported |
| Source package gate | `make test-package` | Three manifest, extraction and publication tests passed |
| Bend model laws | `bend PROOF.bend` / `make test-proof` | 10 modeled laws passed |
| Native CPU measurements | [BENCHMARKS.md](BENCHMARKS.md) | SDK before/after and Bend model measured |

The pre-import baseline on this host passed 12 source smoke checks and 57
codec checks. That baseline is a regression reference, not evidence that the
updated engine passed the same tests.

## What the gates cover

The source smoke gate exercises lifecycle and invalid arguments, secure zeroing,
plain/password/hybrid archive round trips, metadata, extraction ceilings and
key-file behavior. The codec integration gate exercises serial and parallel
compression, exact output sizing, malformed/truncated frames and wrapper policy.
The 74 engine checks cover AIT, explicit trusted legacy migration and path
handling, among other engine behavior. Seven path checks include execute-only
ancestor directories, symlink traversal rejection and final directory syncing. Independent CLI interoperability covers automatic codec
selection, explicit VaptVupt level 9, solid archives, encrypted deduplication
with actual references, and native hybrid public-key archives.
See the test source and final results above for exact coverage and counts.

The public ABI keeps `ZUPTSDK_1.0` and `ZUPTSDK_1.1` and adds the legacy-AIT
policy setter under `ZUPTSDK_1.2`. Internal codec and engine symbols must remain
hidden. The default library name and installed header directory are distinct
from the frozen full SDK.

Release downloads must pass SHA-256 and GPG verification, `zupt test`, and
extraction checks. Archive identifiers and creation times vary, so package
verification compares expected payload content rather than requiring repeated
`.zupt` files to have identical hashes. Newly generated packages use `.zupt`;
historical releases and formats are preserved.

The current timing and taint diagnostics compile the production ML-KEM,
Keccak and comparison primitive through the shared conformance harness. The
reported values are dynamic evidence for that compiler, host and workload;
they are not a proof of constant-time execution. The portable AES limitation
is separate and remains applicable. Older CT results and source hashes in
[CT_VERIFICATION.md](CT_VERIFICATION.md) remain scoped to their dated revisions.

## Proof and performance boundary

The 10 laws in [LAWS.bend](LAWS.bend), checked by [PROOF.bend](PROOF.bend),
model count and sum folds over trees, equivalence with flattened folds, and
preservation of exact order and summary when regrouping. Proofs apply to those
modeled definitions and assumptions; they do not automatically prove independently
implemented C routines, the archive parser, cryptography or the operating
system. Native CPU timing must be measured by building and running a native
executable; the default JavaScript runner is not evidence of native speed.

Performance evidence belongs to the exact workload and machine reported in
[BENCHMARKS.md](BENCHMARKS.md). Legacy full-ABI `easy_*` measurements do not
characterize the base archive engine.

## Remaining validation limits

- No independent external cryptographic audit is claimed.
- No complete constant-time proof applies to the default C build; the portable
  AES path has the table-lookup limitation described in [SECURITY.md](SECURITY.md).
- Callback notification delivery, disk backup/restore and comprehensive
  concurrency stress are outside the release gate.
- Linux x86-64 tests do not establish AArch64, Windows, macOS or Android runtime
  compatibility. Disk restore is destructive and requires separate validation.
- Historical full-ABI fuzzing, timing tests, Jasmin type-checking and binding
  results are historical evidence only. They are not new source-release results.
- AIT compatibility does not repair pre-fix ML-KEM key material or make new
  archives readable by the frozen binary.

## Reproduction

```sh
make -j4
make test
make audit-licenses
make test-proof             # optional Bend 2.0.5 toolchain
make test-asan
make clean
make -j4
```

Sanitizer builds replace normal build artifacts, so restore the production
build before installing or packaging. See the README for signed-download and
extraction commands, and the test sources for their own pass criteria.

Copyright 2026 Cristian Cezar Moisés. [Apache-2.0](LICENSE).
