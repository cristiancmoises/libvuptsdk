# libvuptsdk security policy

Applies to **2.1.0-base.1**, the source-built base API with Zupt 5.2.9 and
VaptVupt codec 2.65.11. See [AUDIT.md](AUDIT.md) for release-specific evidence.

## Reporting vulnerabilities

Report suspected vulnerabilities privately to **zupt@riseup.net**. Include the
SDK version, affected operation, platform and a minimal reproducer without
production secrets. Allow coordinated remediation before public disclosure.

The release signing key and download verification procedure are documented in
the [README](README.md#download-verify-and-extract). A release signing key is
not automatically a contact key for encrypted vulnerability reports.

## Supported scope

This release builds `libvuptsdk-base.so.2` and `libvuptsdk-base.a` from source.
The frozen `prebuilt/libvuptsdk.so.2.0.3` has additional APIs whose complete
source is absent. It is excluded from new release packages and is not covered
by current source-build results. Its `easy_*`, XChaCha20-Poly1305, Argon2id,
key-commitment and streaming-crypto descriptions must not be applied to the
base API.

The base API exposes plaintext archives, password archives using PBKDF2-SHA256,
and hybrid public-key archives using ML-KEM-768 and X25519. Encrypted blocks use
AES-256-CTR with HMAC-SHA256. The wrapper explicitly selects PBKDF2; optional
upstream cryptographic providers do not become available merely by importing
the archive engine.

## Integrity and archive compatibility

New output uses Zupt format 1.6. Its archive integrity trailer (AIT) follows the
footer and covers the serialized header and the first 24 footer bytes:

| Archive mode | AIT | Meaning |
|---|---|---|
| Encrypted | HMAC-SHA256 with the archive MAC key | Detects changes to the covered metadata when the key remains secret |
| Plaintext | XXH64 with reserved zero bytes | Detects accidental corruption; provides no cryptographic authenticity |

Encrypted block authentication also binds block prefaces. Payload blocks have
separate checks; the AIT is not a signature over the entire archive and does
not identify a sender. Plaintext block checksums likewise provide no sender
authentication.

Archives without AIT are rejected by default. The additive
`zuptsdk_ctx_set_allow_legacy_no_ait(ctx, 1)` option exists for trusted legacy
migration only. It allows a missing AIT but still verifies one that is present.
Keep it disabled for untrusted uploads. New archives require an updated reader;
the old frozen SDK is not claimed to decode them.

Older ML-KEM implementations had conformance defects corrected before this
release. Keys and encrypted archives produced by those pre-fix implementations
may not interoperate with corrected implementations. Recover such data using
the matching trusted historical implementation, then re-encrypt with a current
build. Do not interpret a compatibility failure as proof of malicious tampering.

Release downloads use a separate mechanism: GPG verifies the signature on
`SHA256SUMS`, and SHA-256 checks the downloaded archive bytes. `zupt test`
provides an additional archive integrity check. Neither a successful plaintext
archive test nor a checksum downloaded without a trusted signature establishes
publisher identity.

## Resource and filesystem boundary

- Contexts default to a 16 GiB decompressed-output ceiling. Use
  `zuptsdk_ctx_set_max_decompressed()` for a smaller application budget. Zero
  disables it. The legacy options setter does not control extraction calls.
- Enforce input-size, CPU, process-memory and temporary-disk budgets separately.
  Callback I/O stages data through temporary files and is not constant-memory
  streaming.
- Extract each untrusted archive into a new, application-owned directory. The
  engine verifies each file before publishing it and refuses to replace an
  existing target, including regular files, symlinks and FIFOs. These per-file
  protections do not provide a transaction across an entire archive; discard
  the request directory if any operation fails.
- Private temporary files and saved keys use restricted permissions in the
  supported POSIX paths. Final-component symlink rejection is not a general
  guarantee against every hostile filesystem or a compromised parent directory.
- Secure buffers attempt memory locking and are explicitly cleared on
  destruction. OS limits may prevent locking; this does not protect secrets
  from an attacker who controls the process or endpoint.
- Use a context for one active operation at a time. Configure the global
  allocator once at startup. Comprehensive race-detector coverage for distinct
  contexts remains outside the release evidence.

See the bilingual [API reference](doc/API_REFERENCE.md) for a service request
lifecycle and ownership rules.

## Known limitations

1. The portable AES implementation uses secret-indexed table lookups. It is not
   claimed constant-time against cache-timing attackers sharing hardware.
   Optional Jasmin sources are not linked by the default build, and historical
   type-checking results do not establish the current binary's timing behavior.
2. Encrypted archives do not provide forward secrecy after compromise of the
   recipient's long-term private key. CTR encryption also depends on correct
   nonce handling; the construction is not advertised as nonce-misuse-resistant.
3. Cryptographic conformance checks, round trips, sanitizer runs and malformed
   input tests do not establish a proof of the protocol or defect-free parsing.
   An independent external cryptographic audit has not been performed.
4. Progress and log callback setters are reserved: the wrapper stores them but
   does not currently deliver notifications. Use return codes and
   `zuptsdk_last_error_detail()`. Engine error diagnostics may reach stderr.
5. Disk backup and restore are exported but are outside the automated release
   gate. Restore is destructive and provides no interactive confirmation.
6. Current runtime evidence is for Linux x86-64. Architecture-selection code is
   not runtime validation of AArch64, Windows, macOS or Android.
7. Historical full-ABI bindings, fuzz results and benchmarks are not assurance
   for this source release. The frozen prebuilt keeps its historical license
   and implementation limitations.

## Verification scope

[AUDIT.md](AUDIT.md) records the commands and results for this release.
[BENCHMARKS.md](BENCHMARKS.md) separates current measurements from legacy data.
Bend proofs apply only to the modeled count/sum tree folds and their stated
assumptions; they do not prove the C codec, cryptographic implementation,
operating system or archive parser correct.

Copyright 2026 Cristian Cezar Moisés. This document is licensed under
[Apache-2.0](LICENSE).
