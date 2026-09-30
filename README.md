# libvuptsdk

[![License: Apache-2.0](https://img.shields.io/badge/License-Apache--2.0-blue.svg)](LICENSE)
[Português do Brasil](README.pt-BR.md)

**A C SDK for creating, verifying and extracting Zupt archives.**

The source release **2.1.0-base.2** integrates the **Zupt 5.2.10** archive
engine and **VaptVupt codec 2.65.13**. It provides shared and static libraries,
an opaque C API and a `vuptsdk-base` pkg-config module. Zupt, the VaptVupt
application, the standalone codec and this SDK remain separate projects.

Copyright 2026 Cristian Cezar Moisés. First-party source is licensed under
[Apache-2.0](LICENSE); retained third-party notices are listed in [NOTICE](NOTICE).

## Release and compatibility

| Artifact | Version | Scope |
|---|---|---|
| `libvuptsdk-base.so.2` / `libvuptsdk-base.a` | 2.1.0-base.2 | Current source archive API; ABI versions `ZUPTSDK_1.0`, `1.1`, `1.2` |
| `prebuilt/libvuptsdk.so.2.0.3` | Frozen 2.0.3 | Historical full ABI; excluded from installation and release packages |

The base release is a prerelease. The frozen binary has additional `easy_*`,
metrics and streaming-crypto functions whose complete source is absent from
this repository. The historical bindings target that binary and are not base
bindings. Source updates do not update the frozen binary.

New archives use Zupt format **1.6**, including an archive integrity trailer
(AIT), and require an updated reader. Reading trusted older archives without
AIT requires an explicit context option; see the
[migration guide](doc/API_REFERENCE.md#archive-compatibility).

## Build, test and install

Requirements: a C11 compiler, GNU Make, binutils, pthreads, Python 3 for the
test/package tooling, and `pkg-config` for application builds. Zupt is needed to extract `.zupt` release downloads;
it is not an SDK runtime dependency. If your compiler has no `cc` alias, append
`CC=gcc` (or `CC=clang`) to each `make` command and use that compiler in place
of `cc` below.

```sh
make -j4
make test
sudo make install
sudo ldconfig                 # Linux shared-library cache
```

The default prefix is `/usr/local`; use `make install PREFIX=/your/prefix`
for another location. The public header is installed under
`include/libvuptsdk-base/`, separate from the legacy SDK.

```sh
pkg-config --modversion vuptsdk-base   # 2.1.0-base.2
cc doc/example.c $(pkg-config --cflags --libs vuptsdk-base) -o example
./example
```

The [example](doc/example.c) compresses a buffer with VaptVupt, verifies the
archive and checks byte-for-byte extraction. The [API reference](doc/API_REFERENCE.md)
covers ownership, encrypted archives, backend integration and migration.

To run AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
make test-asan
make clean
make -j4                     # restore the normal build before installing
```

The x86-64 default targets the architecture baseline. `VV_SIMD_FLAGS=-mavx2`
is an explicit choice that makes the whole codec artifact require AVX2.
Runtime support on other platforms is limited to the evidence in [AUDIT.md](AUDIT.md).

Optional model checks use `make test-proof` with Bend 2.0.5. The laws prove
properties of modeled count/sum tree folds; they do not prove the C SDK or
cryptography. CPU model measurements are recorded in [BENCHMARKS.md](BENCHMARKS.md).

## Download, verify and extract

Releases are published on the four [repository hosts](#repositories-and-provenance).
New packages use `.zupt` starting with 2.1.0-base.1; earlier releases and
packages retain their original names and formats. The main downloads are:

- `libvuptsdk-base-2.1.0-base.2-src.zupt`: source, tests and product documentation.
- `libvuptsdk-base-2.1.0-base.2-linux-x86_64.zupt`: Linux x86-64 library, header and installation script.
- `SHA256SUMS`, `SHA256SUMS.asc` and `release-key.asc`: checksums, detached signature and public signing key.

Check the public key fingerprint against a trusted copy before importing it:

```text
0CFA 43B9 AA96 42EA AF2B E983 C4C6 61C9 ECFB 46E8
```

```sh
gpg --show-keys --with-fingerprint release-key.asc
gpg --import release-key.asc
gpg --verify SHA256SUMS.asc SHA256SUMS
sha256sum --ignore-missing --check SHA256SUMS
```

Confirm that each downloaded archive reports `OK`. Then use Zupt 5.2.10 or a
compatible newer reader. Use a fresh destination; extraction refuses to replace
existing files. Extraction options must precede the archive name:

```sh
zupt test libvuptsdk-base-2.1.0-base.2-src.zupt
zupt extract -o ./source libvuptsdk-base-2.1.0-base.2-src.zupt
cd source/libvuptsdk-base-2.1.0-base.2
make -j4
make test
```

If Zupt 5.2.10 reports output-path permission errors, use the
[temporary-directory workaround](doc/TROUBLESHOOTING.md#zupt-529-output-directory-permissions--permissões-do-diretório-de-saída).

For the binary bundle, verify its checksum as above, then:

```sh
zupt test libvuptsdk-base-2.1.0-base.2-linux-x86_64.zupt
zupt extract -o ./binary libvuptsdk-base-2.1.0-base.2-linux-x86_64.zupt
cd binary/libvuptsdk-base-2.1.0-base.2-linux-x86_64
sudo sh install.sh
```

The binary requires **Linux x86-64 and glibc 2.34 or newer**; it was tested on
glibc 2.41. Its installer defaults to `/usr/local` and accepts a `PREFIX`
override. Build from source when these requirements do not match your system. Debian and RPM package candidates can
also be built with `packaging/build-deb.sh` and `packaging/build-rpm.sh`.

An archive checksum detects corruption; the verified GPG signature identifies
the signer of the checksum list. A signature status badge on a forge is a
separate host feature. `.zupt` containers include a random archive identifier
and creation time, so rebuilding the same payload does not imply identical
archive bytes.

To build new release bundles from a Git checkout:

```sh
make dist
make dist-binary
make test-package
```

These commands require Python 3 and Zupt; binary packaging also uses binutils
`strip` and `readelf`. Select another CLI with `ZUPT=/path/to/zupt make dist`.
The packager uses VaptVupt, solid mode and level 9. Source packaging includes
Git-tracked files, so stage new files before packaging. An extracted source
bundle can be re-exported without Git while its `SOURCE-MANIFEST.json` checksums
still match; use a Git checkout to package modified source. The binary gate
rejects RPATH/RUNPATH and the package tests compare content after extraction.

## Backend integration and security

Use one context per concurrent job, configure an extraction budget with
`zuptsdk_ctx_set_max_decompressed()`, and release returned buffers with
`zuptsdk_free()`. The default extraction ceiling is 16 GiB; choose a smaller
service-specific limit. Input size, process memory, CPU time and temporary disk
space need separate limits. The callback I/O API currently stages data through
temporary files and is not a constant-memory streaming implementation.

The source API supports plaintext, PBKDF2 password and ML-KEM-768/X25519 hybrid
archives. Plaintext archive checksums do not authenticate a sender. Consult
[SECURITY.md](SECURITY.md) for the cryptographic boundary and known limitations;
report vulnerabilities privately to **zupt@riseup.net**.

## Repositories and provenance

| Project or host | Location |
|---|---|
| SDK, canonical | [git.securityops.co](https://git.securityops.co/cristiancmoises/libvuptsdk) |
| SDK mirror | [GitHub](https://github.com/cristiancmoises/libvuptsdk) |
| SDK mirror | [Codeberg](https://codeberg.org/berkeley/libvuptsdk) |
| SDK mirror | [git.securityops.com.br](https://git.securityops.com.br/cristiancmoises/libvuptsdk) |
| VaptVupt application | [vaptvupt](https://git.securityops.co/cristiancmoises/vaptvupt) |
| Standalone codec | [vaptvupt-codec](https://git.securityops.co/cristiancmoises/vaptvupt-codec) |

Imported revisions:

- Zupt **5.2.10**: `3b3b8f494b4bdd3b74aab60388eef1694ef316f8`.
- VaptVupt codec **2.65.13**: `e30dc9329be7cf9f233b1ac0b1fc9ed31f530391`.

The SDK retains adaptations for its public ABI and build. [NOTICE](NOTICE)
records provenance and third-party obligations; the frozen binary retains its
historical licensing. See [CHANGELOG.md](CHANGELOG.md), [AUDIT.md](AUDIT.md),
[BENCHMARKS.md](BENCHMARKS.md) and [troubleshooting](doc/TROUBLESHOOTING.md).
