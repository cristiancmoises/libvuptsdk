# libvuptsdk-base API reference

[Português do Brasil](API_REFERENCE.pt-BR.md)

This reference applies to the source-built `2.1.0-base.1` prerelease and the
installed header `libvuptsdk-base/zuptsdk.h`. The header is authoritative for
signatures and ownership annotations.

The frozen `libvuptsdk.so.2.0.3` has additional `easy_*`, metrics and
streaming-crypto entry points whose source is not in this repository. Those
entry points are not installed or packaged by the base release.

## Linking

```sh
cc app.c $(pkg-config --cflags --libs vuptsdk-base) -o app
```

Before installation, use `-Iinclude`, link to
`build/libvuptsdk-base.so.2.1.0`, and provide a runtime library search path.

## Error and ownership rules

Functions return `ZUPTSDK_OK` (zero) or a negative `zuptsdk_error_t`. Use
`zuptsdk_strerror()` for the stable description and
`zuptsdk_last_error_detail()` for the current thread's detailed error.

| Value | Name | Meaning |
|---:|---|---|
| 0 | `ZUPTSDK_OK` | Success |
| -1 | `ZUPTSDK_ERR_INVALID_ARG` | Invalid pointer, size or enum |
| -2 | `ZUPTSDK_ERR_NO_MEMORY` | Allocation failed |
| -3 | `ZUPTSDK_ERR_IO` | File or callback I/O failed |
| -4 | `ZUPTSDK_ERR_BAD_ARCHIVE` | Invalid or truncated archive |
| -5 | `ZUPTSDK_ERR_BAD_PASSWORD` | Authentication failed in password mode |
| -6 | `ZUPTSDK_ERR_BAD_KEY` | Invalid key material |
| -7 | `ZUPTSDK_ERR_BAD_MAC` | Authentication tag mismatch |
| -8 | `ZUPTSDK_ERR_BAD_VERSION` | Unsupported archive version |
| -9 | `ZUPTSDK_ERR_BAD_CHECKSUM` | Content checksum mismatch |
| -10 | `ZUPTSDK_ERR_BUFFER_TOO_SMALL` | Destination capacity is insufficient |
| -11 | `ZUPTSDK_ERR_NOT_ENCRYPTED` | Operation requires encrypted input |
| -12 | `ZUPTSDK_ERR_PASSWORD_REQUIRED` | A password was not supplied |
| -13 | `ZUPTSDK_ERR_PQ_KEY_REQUIRED` | A private key was not supplied |
| -14 | `ZUPTSDK_ERR_UNSUPPORTED` | Feature unavailable in this build |
| -15 | `ZUPTSDK_ERR_VERSION_MISMATCH` | Runtime is older than requested |
| -16 | `ZUPTSDK_ERR_PATH_TRAVERSAL` | Unsafe archive path |
| -17 | `ZUPTSDK_ERR_TOO_LARGE` | Extraction output exceeds its limit |
| -18 | `ZUPTSDK_ERR_CRYPTO_FAIL` | Cryptographic primitive failed |
| -19 | `ZUPTSDK_ERR_CANCELLED` | Operation cancelled |
| -99 | `ZUPTSDK_ERR_INTERNAL` | Internal invariant failed |

Memory returned through an output pointer must be released with
`zuptsdk_free()`. Opaque objects use their matching `*_destroy()` function.
Do not mix those allocations with the C library's `free()` when a custom
allocator is configured.

## Contexts and extraction policy

```c
zuptsdk_ctx_t *ctx = NULL;
int rc = zuptsdk_ctx_create(&ctx);
if (rc == ZUPTSDK_OK)
    rc = zuptsdk_ctx_set_threads(ctx, 2);
if (rc == ZUPTSDK_OK)
    rc = zuptsdk_ctx_set_max_decompressed(ctx, 256ull * 1024 * 1024);
/* use ctx */
zuptsdk_ctx_destroy(ctx);
```

The extraction ceiling defaults to 16 GiB. A zero value disables it and is
not recommended for untrusted input. The decoder checks both the index's
declared total before creating output files and the bytes actually decoded.
The older `zuptsdk_options_set_max_decompressed()` remains for ABI
compatibility, but extraction calls do not receive an options object; use the
context setter.

A context must not be used by concurrent calls. The global allocator must be
configured once, before any other SDK operation. Dedicated race-detector
coverage for distinct contexts is still missing in this prerelease.

The log and progress setter functions currently store callbacks but do not
invoke them. Normal codec progress and summaries are suppressed by the base
wrapper; the embedded codec can still write an error diagnostic to stderr.

## Compression options

Create an options object with `zuptsdk_options_create()` and release it with
`zuptsdk_options_destroy()`. The defaults are automatic codec selection,
level 7, no deduplication and non-solid archives.

Available codec values are `ZUPTSDK_CODEC_AUTO`, `VAPTVUPT`, `LZHP`, `LZH`,
`LZ`, and `STORE`. Setters validate level 1 through 9 and the codec's supported
block-size range. The `AUTO` choice is hardware-adaptive; explicitly choose
`ZUPTSDK_CODEC_VAPTVUPT` when the archive must use VaptVupt.

## Embedded codec policy

The SDK wrapper maps levels 1–2 to `ULTRA_FAST`, 3–7 to `BALANCED`, and 8–9
to `EXTREME`. Balanced and extreme modes enable automatic BCJ filtering.
`format_v2=0` is the codec's automatic selection policy; it does not force every
frame into an old format. Use current readers for new archives.

Nested frame checksums are disabled because the enclosing archive block has
its own integrity check. Compression also decodes the candidate frame and
compares its exact bytes with the input; a rejected candidate lets the caller
store an uncompressed block. Internal `vvz_*` functions are not public API and
must not be used without the archive's integrity checks.

## Buffer archive example

```c
#include <string.h>
#include <zuptsdk.h>

int main(void)
{
    static const unsigned char input[] = "VaptVupt archive payload";
    zuptsdk_ctx_t *ctx = NULL;
    zuptsdk_options_t *opts = NULL;
    unsigned char *archive = NULL, *output = NULL;
    size_t archive_size = 0, output_size = 0;
    int rc = zuptsdk_ctx_create(&ctx);

    if (rc == 0) rc = zuptsdk_options_create(&opts);
    if (rc == 0) rc = zuptsdk_options_set_codec(opts, ZUPTSDK_CODEC_VAPTVUPT);
    if (rc == 0) rc = zuptsdk_compress_buffer(
        ctx, opts, "payload.txt", input, sizeof(input) - 1,
        NULL, NULL, &archive, &archive_size);
    if (rc == 0) rc = zuptsdk_verify(ctx, archive, archive_size, NULL, NULL);
    if (rc == 0) rc = zuptsdk_extract_buffer(
        ctx, archive, archive_size, NULL, NULL, &output, &output_size);

    int ok = rc == 0 && output_size == sizeof(input) - 1 &&
             memcmp(input, output, output_size) == 0;
    zuptsdk_free(output);
    zuptsdk_free(archive);
    zuptsdk_options_destroy(opts);
    zuptsdk_ctx_destroy(ctx);
    return ok ? 0 : 1;
}
```

`zuptsdk_compress_files()` accepts filesystem paths. `zuptsdk_extract_to_dir()`
extracts every safe entry. `zuptsdk_compress_buffer()` and
`zuptsdk_extract_buffer()` are intended for a single logical file. The
extraction functions reject unsafe relative and absolute paths in the core.
Verified files are published without replacing existing destinations: a name
conflict with a file, symlink or FIFO fails the operation and preserves the
existing target. Use a fresh extraction directory. This is a per-file rule,
not a transaction over the entire archive.

## Callback I/O

`zuptsdk_compress_stream()` and `zuptsdk_decompress_stream()` accept read and
write callbacks. In this prerelease they stage data through owner-only
temporary files; they are callback-oriented wrappers, not constant-memory
streaming implementations. The decompression path uses the context's output
ceiling.

## Secure buffers and keys

Passwords are passed with `zuptsdk_secure_buf_t`. Creation attempts to lock
the pages in memory, and destruction explicitly clears them, but OS resource
limits can prevent page locking. Keypair, public-key and private-key handles
use matching destroy functions. On tested Linux systems, saving a private key
establishes mode 0600 before writing, refuses a final-component symbolic link,
and rejects non-regular output targets.

The source build supports PBKDF2 password archives and ML-KEM-768/X25519
hybrid archives. The wrapper explicitly selects PBKDF2; optional upstream
Argon2 support is not exposed as a base API setting. Pass a secure password
buffer or recipient key handle to the archive operations, and release it with
its matching destroy function. The full `easy_*` encryption interface remains
exclusive to the frozen compatibility binary. Read [SECURITY.md](../SECURITY.md)
for the precise cryptographic and compatibility limits.

## Metadata

`zuptsdk_archive_info_read()` reads header metadata without extracting
payloads. It reports format version, creation time, archive UUID, archive byte
size, flags and a structurally valid footer's block count. The legacy
block-count getter is 32-bit and saturates at `UINT32_MAX`. Destroy the object with
`zuptsdk_archive_info_destroy()`.

## Disk operations

`zuptsdk_disk_backup()` and `zuptsdk_disk_restore()` are exported by the base
library but are not part of the current automated release gate. Restore is
destructive and performs no interactive confirmation. Test these functions
only against disposable image files or virtual machines, never a production
block device.

## Current validation boundary

The release gate exercises context/options lifecycle, secure zeroing, plain,
password-authenticated and hybrid-key VaptVupt archive round trips, malformed
solid input, metadata decoding, the extraction ceiling, archive integrity
trailers, embedded-codec checks, per-file licensing, and sanitizer builds. Callback notification, disk
operations, Windows and macOS runtime behavior are not claimed by that gate.


## Archive compatibility

Version 2.1.0-base.1 writes Zupt format 1.6, with an archive integrity trailer
(AIT) after the footer and authenticated block prefaces in encrypted archives.
Use an updated reader for new archives; the frozen 2.0.3 binary is not a
compatible replacement reader.

By default, the context rejects archives without AIT. When migrating a
**trusted** older archive, create a dedicated context and explicitly enable:

```c
int rc = zuptsdk_ctx_set_allow_legacy_no_ait(ctx, 1);
/* Check rc, then verify/extract only the trusted legacy archive. */
```

The setter accepts only 0 or 1 and is exported under `ZUPTSDK_1.2`. Zero is the
default. This option permits a missing trailer; it does not disable verification
of a trailer that is present. Prefer rewriting migrated content as a current
archive and keep the option disabled for incoming untrusted data.

The AIT protects the serialized header and footer prefix. For encrypted
archives it uses HMAC-SHA256 with the archive MAC key; for plaintext archives
it uses XXH64 and provides corruption detection only. Payload blocks have their
own integrity checks. Neither a plaintext checksum nor metadata parsing proves
who created an archive. Release download signatures are separate GPG signatures
over `SHA256SUMS`, described in the [README](../README.md#download-verify-and-extract).

## Integrating a backend service

Use the source API through a native C/C++ module or an FFI adapter that links
`vuptsdk-base`. The existing Python, Node.js, Go and Rust bindings target the
historical full ABI and cannot be pointed at this library as a drop-in upgrade.
The installed `zuptsdk.h` is the public contract; internal `vvz_*` and engine
symbols are hidden and are not backend integration points.

A practical request lifecycle is:

1. Enforce an upload-size limit before passing bytes to the SDK. Create a
   context for the job and set its thread count and decompressed-byte ceiling.
2. Run blocking compression or extraction in a bounded worker queue. Account
   for SDK worker threads when sizing the service's own concurrency.
3. Use a fresh, application-owned extraction directory for each request and
   separate quotas for temporary disk space, CPU time and process memory.
   The decompressed-byte ceiling is not an input or memory budget.
4. Check every return code. Handle `ZUPTSDK_ERR_TOO_LARGE`, authentication and
   malformed-input errors as failed jobs; expose application messages to the
   client and keep `zuptsdk_last_error_detail()` in appropriate server logs.
5. Publish output only after the operation succeeds. On failure, discard the
   request directory; extraction does not provide a transaction across all
   entries in an archive.
6. Free SDK buffers with `zuptsdk_free()` and destroy options, key handles,
   password buffers and the context. Configure a custom global allocator only
   once at process startup, before any SDK calls.

The [buffer example](#buffer-archive-example) is suitable for bounded payloads.
Use the filesystem API for larger archives. Callback I/O still stages data in
temporary files and may load buffers into memory; progress and log callback
registration does not currently deliver notifications. A shared context must
never serve simultaneous calls, and this release does not claim comprehensive
race-detector validation of separate contexts.

## License

Copyright 2026 Cristian Cezar Moisés. First-party source and this documentation
use [Apache-2.0](../LICENSE). Preserve the third-party notices in [NOTICE](../NOTICE).
Historical binary and binding references do not change the frozen binary's
license or provide source for its additional API.
