# libzuptsdk troubleshooting

Common issues and how to diagnose them.

---

## Build / link errors

### `error while loading shared libraries: libzuptsdk.so.2`

The runtime linker can't find the library. Choose one:

```bash
# Permanent (system-wide, requires root):
sudo ldconfig

# Permanent (user, no root):
echo "/path/to/libzuptsdk/lib" >> ~/.ld-library-path

# Per-invocation:
LD_LIBRARY_PATH=/path/to/libzuptsdk/lib ./your_app

# Build-time bake-in (recommended for portable apps):
cc app.c -lzuptsdk -Wl,-rpath,/path/to/libzuptsdk/lib
```

### `cannot find -lzuptsdk`

The linker can't find the library at link time. Use `pkg-config`:

```bash
cc app.c $(pkg-config --cflags --libs zuptsdk) -o app
```

If pkg-config can't find it either, set `PKG_CONFIG_PATH`:

```bash
PKG_CONFIG_PATH=/opt/zupt/lib/pkgconfig pkg-config --cflags --libs zuptsdk
```

### `undefined reference to zuptsdk_easy_encrypt`

You're linking against the source-only build (`libzuptsdk-base.so.2`)
which lacks the `easy_*` layer. The `easy_*` functions live in the
canonical prebuilt only. Link against `libzuptsdk.so.2` (no `-base`):

```bash
# Wrong (source build, missing easy_*):
cc app.c -lzuptsdk-base
# Right (canonical, has full ABI):
cc app.c -lzuptsdk
```

`make install` always installs the canonical, so this is only an issue
if you're using the in-tree build artifacts directly.

### `fatal error: zuptsdk.h: No such file or directory`

The compiler can't find the headers. Use pkg-config:

```bash
cc -I$(pkg-config --variable=includedir zuptsdk) app.c ...
# or simpler:
cc $(pkg-config --cflags zuptsdk) app.c ...
```

### Python: `RuntimeError: libzuptsdk shared library not found`

The Python bindings tried `libzuptsdk.so.2`, `libzuptsdk.so`, and
`ctypes.util.find_library('zuptsdk')` and none worked. Set:

```bash
export ZUPTSDK_LIBRARY=/path/to/libzuptsdk.so.2.0.0
```

Or call `make install` to put it in `/usr/local/lib`, then `sudo ldconfig`.

### Node.js: `Error: koffi.load() failed`

Same as the Python case — set `ZUPTSDK_LIBRARY` env var.

---

## Runtime errors

### `decrypt: authentication failed (-10)`

The MAC verification step failed. Three possible causes:

1. **The blob has been tampered with**, even by a single bit.
2. **You're using the wrong private key**. Cross-check by comparing the
   pubkey fingerprint of the privkey to the pubkey used by the sender.
3. **The blob was encrypted with a different version of libzuptsdk**
   (rare — the format includes a version byte that gets checked first).

### `decrypt: format error (-11)`

The input is not a valid libzuptsdk blob. It may be:
- An empty file
- Truncated mid-transfer
- Compressed / wrapped in some other format
- A regular text file (libzuptsdk blobs are binary)

Check the first 8 bytes — they should start with the libzuptsdk magic
header.

### Decryption is slow

If your message is small (≤ 1 KB), most of the time is the public-key
KEM operation (~430 μs constant overhead). This is fundamental to PQ
hybrid cryptography. If you need higher throughput for many small
messages:

- Reuse the recipient's loaded `zuptsdk_privkey_t` across calls (use the
  lifecycle API, not `easy_*`) — saves the file-read on each call.
- For internal services where you control both sides, switch to field
  encryption (`encrypt_field`) with a shared symmetric key — drops the
  per-call cost from 430 μs to 5 μs.

If your message is large (≥ 1 MB), the throughput should be ~150-200 MB/s
on a modern 2-core. If you're seeing less:

- Check `cat /proc/cpuinfo` for AES-NI (`aes` flag). Without AES-NI, the
  AEAD path falls back to bitsliced AES, which is ~5× slower.
- Check that you're not unintentionally building with `-O0` (debug). The
  release build uses `-O2`.

### `mlock` warnings on resource-constrained systems

```
mlock: Cannot allocate memory
```

The library tries to `mlock()` private-key buffers to keep them out of
swap. On systems with low `RLIMIT_MEMLOCK` (typical default: 64 KB), this
warning is harmless — the library falls back to non-locked memory. To
silence it:

```bash
ulimit -l 65536      # raise to 64 MiB
# or systemd: LimitMEMLOCK=infinity in service unit
```

The library will continue to function correctly with degraded protection.

### `Argon2id is too slow on my server`

Default Argon2id parameters (m=64MB, t=3, p=1) take ~1 second on a 2-CPU
sandbox. For high-throughput web servers, you may want to lower this.
Use the lifecycle API to set custom params:

```c
zuptsdk_password_params_t *params;
zuptsdk_password_params_create(&params);
zuptsdk_password_params_set_custom(params, /*memory_kb=*/16384, /*t=*/2, /*p=*/2);
/* … pass params to a custom encrypt … */
zuptsdk_password_params_destroy(params);
```

Trade-off: lower parameters make brute-force easier. Don't go below
m=8MB, t=2 for password storage.

---

## Cross-compilation

### Building for AArch64 from x86_64

```bash
sudo apt install gcc-aarch64-linux-gnu
make clean
make CC=aarch64-linux-gnu-gcc
```

The Makefile auto-detects target arch via `$(CC) -dumpmachine` and
selects NEON SIMD instead of x86 SSE/AVX2.

### Building for Termux (Android AArch64)

```bash
pkg install clang make
make CC=aarch64-linux-android-clang
```

Note: the canonical prebuilt is x86_64 only. On Termux, you must use the
source build (which lacks `easy_*`). Track the open item in AUDIT.md.

---

## Memory leaks

If ASAN/Valgrind reports leaks from libzuptsdk, the most common cause is
forgetting to free output buffers from the `easy_*` API:

```c
/* Wrong: leaks blob */
uint8_t *blob = NULL; size_t sz = 0;
zuptsdk_easy_encrypt(pub, msg, len, &blob, &sz);
write(fd, blob, sz);
/* missing: free(blob) */

/* Right */
uint8_t *blob = NULL; size_t sz = 0;
zuptsdk_easy_encrypt(pub, msg, len, &blob, &sz);
write(fd, blob, sz);
free(blob);                /* or zuptsdk_free(blob) — equivalent */
```

For lifecycle objects, every `*_create` needs a `*_destroy`:

| Created by | Destroyed by |
|---|---|
| `zuptsdk_ctx_create` | `zuptsdk_ctx_destroy` |
| `zuptsdk_keypair_generate` | `zuptsdk_keypair_destroy` |
| `zuptsdk_secure_buf_create` | `zuptsdk_secure_buf_destroy` |
| `zuptsdk_stream_pq_init_*` | `zuptsdk_stream_state_destroy` |

---

## Reporting bugs

If you've ruled out the above and believe you've found a real bug:

1. **Reduce to a minimal test case** — ideally < 50 lines.
2. **Include the output of `zuptsdk_version_string()`**.
3. **Include your platform**: `uname -a`, `gcc --version`, `glibc`/`musl` version.
4. **Run with ASAN** if the bug involves crashes or memory:
   ```bash
   make test-asan
   ```
5. **For security issues**: email `zupt@riseup.net` directly. Do not file
   public GitHub issues.

For non-security bugs: <https://github.com/cristiancmoises/libzuptsdk/issues>

---

**License**: This document is part of the libzuptsdk project, licensed under the GNU Affero General Public License version 3 or later (AGPL-3.0-or-later). See [LICENSE](../LICENSE).
