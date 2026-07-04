# libvuptsdk API reference

This document describes every public function with full signature, parameter
semantics, return values, and a runnable example.

For the cryptographic background of each operation, see [SECURITY.md](../SECURITY.md).
For the convenience layer used by most application code, start with the
"Easy API" section. For low-level ABI, see "Lifecycle API".

---

## Easy API (`<zuptsdk_easy.h>`)

The recommended interface. 11 functions cover 95% of application use cases.

### Keypair generation

```c
int zuptsdk_easy_keygen(const char *pubkey_out_path,
                        const char *privkey_out_path);
```

Generates a fresh ML-KEM-768 + X25519 hybrid keypair and writes the public
and private parts to two files. Files are created with the OS umask
(typically `0644` for pubkey, you may want to `chmod 0600` the privkey).

**Parameters**:
- `pubkey_out_path` — file to write the public key (~1.2 KB)
- `privkey_out_path` — file to write the private key (~3.7 KB; **keep secret**)

**Returns**: `0` on success, non-zero on error. Use `zuptsdk_strerror(rc)`
to get a printable message.

**Performance**: ~470 μs on 2.8 GHz (see [BENCHMARKS.md](../BENCHMARKS.md)).

**Example**:
```c
if (zuptsdk_easy_keygen("alice.pub", "alice.priv") != 0) {
    fprintf(stderr, "keygen failed: %s\n", zuptsdk_strerror(rc));
    return 1;
}
chmod("alice.priv", 0600);  /* harden permissions */
```

---

### Public-key encryption

```c
int zuptsdk_easy_encrypt(const char *recipient_pubkey_path,
                         const uint8_t *plaintext, size_t plaintext_sz,
                         uint8_t **blob_out, size_t *blob_sz);
```

Encrypts `plaintext` (any length, including 0) for the recipient identified
by `recipient_pubkey_path`. The output is a self-contained blob containing
the KEM ciphertext, the ECDH ciphertext, the AEAD ciphertext, the AEAD tag,
the BLAKE2b key-commitment tag, and protocol-version bytes.

**Parameters**:
- `recipient_pubkey_path` — path to a `.pub` file (created by `easy_keygen`)
- `plaintext` — bytes to encrypt
- `plaintext_sz` — number of bytes (may be 0)
- `blob_out` — output: pointer set to a heap-allocated blob
- `blob_sz` — output: size of the blob in bytes

**Returns**: `0` on success. The caller MUST `free(*blob_out)` after use.

**Performance**: ~430 μs constant + ~6 ns/byte after that (see BENCHMARKS).

**Example**:
```c
const char *msg = "Hello, Alice!";
uint8_t *blob = NULL;
size_t blob_sz = 0;
int rc = zuptsdk_easy_encrypt("alice.pub",
                               (const uint8_t *)msg, strlen(msg),
                               &blob, &blob_sz);
if (rc != 0) { /* handle error */ }
/* ... use blob ... */
free(blob);
```

---

### Public-key decryption

```c
int zuptsdk_easy_decrypt(const char *recipient_privkey_path,
                         const uint8_t *blob, size_t blob_sz,
                         uint8_t **plaintext_out, size_t *plaintext_sz);
```

Decrypts a blob produced by `easy_encrypt` using the recipient's private
key. Returns failure (non-zero rc) on:
- Tampered ciphertext (MAC verify fail) — guaranteed detection
- Wrong private key — surfaces as a MAC/commitment failure. ML-KEM-768 uses
  FIPS 203 **implicit rejection**: decapsulation always succeeds and returns a
  pseudorandom shared secret for a bad key/ciphertext, so the mismatch is
  caught by the downstream AEAD MAC, not by a distinct "KEM decapsulation
  failure" code.
- Truncated/malformed blob (early-reject)

**Constant-time guarantees**: MAC-fail vs. valid-decrypt timing differs by
<2% (see AUDIT.md "Test 7 — Side-channel timing variance"). The library
does not leak *why* a decrypt failed to a network observer.

**Parameters**:
- `recipient_privkey_path` — path to a `.priv` file
- `blob` — ciphertext from `easy_encrypt`
- `blob_sz` — size of the blob
- `plaintext_out` — output: pointer set to heap-allocated plaintext
- `plaintext_sz` — output: plaintext size in bytes

**Returns**: `0` on success. The caller MUST `free(*plaintext_out)`.

**Example**:
```c
uint8_t *out = NULL;
size_t out_sz = 0;
int rc = zuptsdk_easy_decrypt("alice.priv", blob, blob_sz, &out, &out_sz);
if (rc != 0) {
    /* MAC failed, wrong key, or invalid blob */
    fprintf(stderr, "decrypt: %s\n", zuptsdk_strerror(rc));
    return 1;
}
fwrite(out, 1, out_sz, stdout);
free(out);
```

---

### Password-based encryption

```c
int zuptsdk_easy_encrypt_password(const char *password,
                                  const uint8_t *plaintext, size_t plaintext_sz,
                                  uint8_t **blob_out, size_t *blob_sz);

int zuptsdk_easy_decrypt_password(const char *password,
                                  const uint8_t *blob, size_t blob_sz,
                                  uint8_t **plaintext_out, size_t *plaintext_sz);
```

Encrypts/decrypts using a password. The password is run through Argon2id
with RFC 9106 IETF parameters (memory=64 MiB, t=3, p=1) producing a 32-byte
key, which is then used with AEAD. The Argon2id salt is generated fresh
per encrypt and stored in the blob header.

**Performance warning**: ~1.1 sec per call on 2-CPU sandbox; expect
~250-400 ms on modern desktop hardware. This is by design — the cost
forces password-guessing attackers to spend the same time per guess.

**Returns**: `0` on success.

---

### Field-level encryption

```c
int zuptsdk_easy_encrypt_field(const uint8_t key[32],
                               const char *plaintext,
                               char **b64_out);

int zuptsdk_easy_decrypt_field(const uint8_t key[32],
                               const char *b64_input,
                               char **plaintext_out);
```

For high-throughput scenarios (database column encryption, JSON field
encryption, etc.) where a 32-byte symmetric key is already available.
Output is base64-encoded and safe for TEXT columns.

**Caller MUST `free(*b64_out)` and `free(*plaintext_out)`.**

**Performance**: ~5 μs per encrypt — suitable for high-volume use.

**Typical pattern**:
```c
/* At application startup: */
uint8_t salt[16], master_key[32];
zuptsdk_easy_random_salt(salt);  /* persist `salt` somewhere */
zuptsdk_easy_derive_key("master passphrase", salt, master_key);

/* For each field, no per-call KDF cost: */
char *email_ct = NULL;
zuptsdk_easy_encrypt_field(master_key, "alice@example.com", &email_ct);
/* email_ct is base64; safe to store in a TEXT column */
free(email_ct);

/* When the application shuts down: */
zuptsdk_secure_zero(master_key, sizeof(master_key));
```

---

### File encryption (streamed)

```c
typedef void (*zuptsdk_easy_progress_t)(uint64_t bytes_done,
                                         uint64_t bytes_total,
                                         void *userdata);

int zuptsdk_easy_encrypt_file(const char *recipient_pubkey_path,
                              const char *input_path,
                              const char *output_path,
                              zuptsdk_easy_progress_t cb, void *userdata);

int zuptsdk_easy_decrypt_file(const char *recipient_privkey_path,
                              const char *input_path,
                              const char *output_path,
                              zuptsdk_easy_progress_t cb, void *userdata);
```

Streamed file encryption / decryption. No size limit (uses streaming AEAD
with periodic re-keying). Memory usage is constant (~64 KB) regardless of
file size.

**Progress callback**: pass `NULL` if not needed.

**Atomicity**: the output file is fully written before returning. If
encryption fails midway, the output file may exist but be incomplete —
the caller should `unlink()` it on error.

---

### Key derivation utilities

```c
int zuptsdk_easy_random_salt(uint8_t out[16]);
```
Fills `out` with 16 cryptographically random bytes from the OS CSPRNG.
Returns `0` on success.

```c
int zuptsdk_easy_derive_key(const char *password,
                            const uint8_t salt[16],
                            uint8_t key_out[32]);
```
Derives a deterministic 32-byte key from `password` + `salt` via Argon2id.
Use this once at startup to obtain a long-lived field-encryption key.

**Performance**: same as `easy_encrypt_password` — ~1 sec.

---

## Lifecycle API (`<zuptsdk.h>`)

For applications needing fine-grained control: custom thread count, progress
callbacks, secure-buffer keys, custom allocators, structured logging.

### Version + error reporting

```c
const char *zuptsdk_version_string(void);   /* e.g. "2.1.5" */
int  zuptsdk_version_check(int major, int minor, int patch);
const char *zuptsdk_strerror(int code);
const char *zuptsdk_last_error_detail(void);  /* TLS-stored details */
```

### Context lifecycle

```c
int  zuptsdk_ctx_create(zuptsdk_ctx_t **ctx_out);
void zuptsdk_ctx_destroy(zuptsdk_ctx_t *ctx);
int  zuptsdk_ctx_set_log_callback(zuptsdk_ctx_t *ctx,
                                   void (*cb)(int level, const char *msg, void *ud),
                                   void *userdata);
int  zuptsdk_ctx_set_progress_callback(zuptsdk_ctx_t *ctx,
                                        void (*cb)(uint64_t done, uint64_t total, void *ud),
                                        void *userdata);
```

### Streaming encryption

```c
int zuptsdk_stream_pq_init_encrypt(zuptsdk_ctx_t *ctx,
                                    const zuptsdk_pubkey_t *recipient,
                                    zuptsdk_stream_state_t **state_out);
int zuptsdk_stream_pq_init_decrypt(zuptsdk_ctx_t *ctx,
                                    const zuptsdk_privkey_t *recipient,
                                    zuptsdk_stream_state_t **state_out);
int zuptsdk_stream_chunk_encrypt(zuptsdk_stream_state_t *state,
                                  const uint8_t *plaintext, size_t plaintext_sz,
                                  uint8_t **chunk_out, size_t *chunk_sz);
int zuptsdk_stream_chunk_decrypt(zuptsdk_stream_state_t *state,
                                  const uint8_t *chunk, size_t chunk_sz,
                                  uint8_t **plaintext_out, size_t *plaintext_sz);
int zuptsdk_stream_rekey(zuptsdk_stream_state_t *state);
void zuptsdk_stream_state_destroy(zuptsdk_stream_state_t *state);
```

For continuous streaming (e.g. encrypting a TCP socket). Re-key periodically
to bound the maximum amount of plaintext under a single key.

### Memory utilities

```c
int  zuptsdk_secure_buf_create(size_t size, zuptsdk_secure_buf_t **buf_out);
void zuptsdk_secure_buf_destroy(zuptsdk_secure_buf_t *buf);
int  zuptsdk_secure_buf_get(zuptsdk_secure_buf_t *buf,
                             uint8_t **data_out, size_t *size_out);
void zuptsdk_secure_zero(void *ptr, size_t n);
int  zuptsdk_constant_time_compare(const void *a, const void *b, size_t n);
void zuptsdk_free(void *ptr);   /* matches what easy_* allocated */
```

`zuptsdk_secure_buf_*` allocates `mlock()`-ed memory that is `madvise(MADV_DONTDUMP)`
and zeroed on destroy. Use for keys, passwords, plaintext you want to keep
out of swap and crash dumps.

`zuptsdk_secure_zero` uses a memory-clobber barrier to prevent the
compiler from optimizing the write away.

`zuptsdk_constant_time_compare` is a Jasmin-verified constant-time memcmp.

`zuptsdk_free` MUST be used to release pointers returned by the `easy_*`
API. On Windows or with custom allocators, free() may not match.

### Custom allocator

```c
typedef struct {
    void *(*malloc)(size_t n, void *userdata);
    void *(*calloc)(size_t n, size_t sz, void *userdata);
    void *(*realloc)(void *p, size_t n, void *userdata);
    void  (*free)(void *p, void *userdata);
    void *userdata;
} zuptsdk_allocator_t;

int zuptsdk_set_allocator(const zuptsdk_allocator_t *alloc);
```

Replace the global heap allocator. Must be called before any other libvuptsdk
function. Useful for:
- Embedded systems with custom heaps
- Memory-pool allocation for low-latency
- Tracking allocations in test harnesses

---

## Error codes

| Code | Symbol | Meaning |
|------|--------|---------|
| 0 | `ZUPTSDK_OK` | success |
| -1 | `ZUPTSDK_ERR_GENERIC` | unspecified error (use `last_error_detail()`) |
| -2 | `ZUPTSDK_ERR_INVALID_ARG` | a parameter was NULL or out of range |
| -3 | `ZUPTSDK_ERR_IO` | file not found, permission denied, etc. |
| -4 | `ZUPTSDK_ERR_NO_MEMORY` | malloc failed |
| -5 | `ZUPTSDK_ERR_BUFFER_TOO_SMALL` | output buffer too small |
| -10 | `ZUPTSDK_ERR_BUFFER_TOO_SMALL` | output buffer insufficient |
| -11 | `ZUPTSDK_ERR_NOT_ENCRYPTED` | tried to decrypt an unencrypted archive |
| -12 | `ZUPTSDK_ERR_PASSWORD_REQUIRED` | archive needs a password but none supplied |
| -13 | `ZUPTSDK_ERR_PQ_KEY_REQUIRED` | archive needs a PQ key but none supplied |

> **Authoritative list:** `include/zuptsdk.h` (`zuptsdk_error_t`) is the source
> of truth; the codes above mirror it. Note there is **no** "KEM decapsulation
> failure" code — by design (implicit rejection), a wrong key or tampered
> ciphertext is reported as the MAC/authentication failure `ZUPTSDK_ERR_BAD_MAC`
> (or `ZUPTSDK_ERR_BAD_PASSWORD` for password mode), never as a KEM-level error.

---

## Memory ownership rules

| Function category | Caller responsibility |
|---|---|
| `easy_encrypt` / `easy_decrypt` / `easy_encrypt_password` / `easy_decrypt_password` | `free(*output)` (or `zuptsdk_free`) |
| `easy_encrypt_field` / `easy_decrypt_field` | `free(*output)` |
| `easy_encrypt_file` / `easy_decrypt_file` | none (writes to disk) |
| `easy_keygen` | none (writes to disk) |
| `easy_random_salt` / `easy_derive_key` | none (writes to caller-provided buffer) |
| `ctx_create` / `*_create` / `*_load` | matching `*_destroy` |
| `secure_buf_create` | `secure_buf_destroy` |

**Rule of thumb**: if a function takes `T **out`, the caller owns `*out` and
must release it. If it takes `T *out` (single-pointer to caller buffer), no
ownership transfer.

---

## Threading model

- **Read-only operations** (`encrypt`, `decrypt`, `version_string`,
  `strerror`) are thread-safe and may be called concurrently from any
  number of threads.
- **`zuptsdk_ctx_t` is thread-affine** — do not share a single context
  across threads; create one per worker thread.
- **`zuptsdk_set_allocator` is process-global** — call once before any
  other library function, never thereafter.
- **Stream state** (`zuptsdk_stream_state_t`) is single-thread — protect
  with a mutex if shared.

---

## Compatibility

| Compiler / runtime | Status |
|---|---|
| GCC ≥ 7 | tested CI |
| Clang ≥ 10 | tested CI |
| C++17 (g++/clang++) | all headers have `extern "C"` guards |
| musl libc | tested (Alpine builds work) |
| glibc ≥ 2.17 | required (mlock + getrandom) |
| AArch64 (ARM64) | source build works; canonical prebuilt is x86_64 |
| Win32 / macOS | source build works with adjustments; no prebuilts shipped |

---

**License**: This document is part of the libvuptsdk project, licensed under the GNU Affero General Public License version 3 or later (AGPL-3.0-or-later). See [LICENSE](../LICENSE).
