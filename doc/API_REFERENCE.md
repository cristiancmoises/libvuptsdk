# libvuptsdk-base API reference

[Português do Brasil](API_REFERENCE.pt-BR.md)

This reference applies to the source-built `2.0.4-base.1` prerelease and the
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
`build/libvuptsdk-base.so.2.0.4`, and provide a runtime library search path.

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

The source build exposes the hybrid key/archive functions, but the full
`easy_*` encryption interface remains exclusive to the frozen compatibility
binary. Read `SECURITY.md` before relying on cryptographic modes.

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
solid input, metadata decoding, the extraction ceiling, 57 embedded-codec
checks, per-file licensing, and sanitizer builds. Callback notification, disk
operations, Windows and macOS runtime behavior are not claimed by that gate.
