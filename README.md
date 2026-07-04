# libzuptsdk

[![License: AGPL-3.0+](https://img.shields.io/badge/License-AGPL--3.0--or--later-blue.svg)](LICENSE)
[![ABI: ZUPTSDK_2.1](https://img.shields.io/badge/ABI-ZUPTSDK__2.1-green.svg)](zuptsdk.map)
[![Status: production](https://img.shields.io/badge/status-production-brightgreen.svg)](AUDIT.md)
[![Tests: 30/30](https://img.shields.io/badge/tests-30%2F30%20pass-brightgreen.svg)](AUDIT.md)

**Post-quantum hybrid cryptography for production C, C++, Python, Node.js, Go, and Rust applications.**

`libzuptsdk` is the cryptographic backbone of the [Zupt](https://github.com/cristiancmoises/zupt) backup utility. The library is also designed for **standalone use**: any application that needs strong, future-proof, audited encryption can link against it.

## Why libzuptsdk?

| Concern | What libzuptsdk gives you |
|---|---|
| **Quantum-safe** | ML-KEM-768 (FIPS 203, NIST 2024-standardized) hybrid with X25519 ECDH. Both must fall to break a message. The from-source KEM is verified conformant against the official NIST ACVP vectors (**80/80**) and two independent implementations — see [ML-KEM-768 conformance](#ml-kem-768-conformance-status). |
| **Misuse-resistant** | Default AEAD (XChaCha20-Poly1305) uses random 24-byte nonces — no nonce-reuse risk. AES-256-SIV available for nonce-misuse-resistant mode. |
| **Committing AEAD** | BLAKE2b key-commitment defeats the multi-key partitioning attacks that affect raw AEAD constructions (Albertini et al. 2022). |
| **Constant-time** | Critical primitives (AES, X25519, MAC compare, ML-KEM cmov-select) are written in [Jasmin](https://github.com/jasmin-lang/jasmin) and verified constant-time by the type system. |
| **Forward secrecy** | Per-archive ephemeral KEM session — past traffic stays protected even if long-term keys leak later. |
| **Memory-hardened** | `mlock()` on private keys, explicit zeroization on free, RELRO, BIND_NOW, stack canaries, FORTIFY_SOURCE. |
| **Stable C ABI** | Symbol-versioned (`ZUPTSDK_1.0`, `ZUPTSDK_2.1`) — old binaries keep working forever. |
| **Multi-language** | Native bindings: Python, Node.js, Go, Rust, C, C++. |

## Quick install

### Debian / Ubuntu / Mint

```bash
sudo apt install ./libzuptsdk2_2.0.0_amd64.deb ./libzuptsdk-dev_2.0.0_amd64.deb
```

### Fedora / RHEL / openSUSE

```bash
tar -xzf libzuptsdk-2.0.0.srpm.tar.gz
rpmbuild -bb SPECS/libzuptsdk.spec
sudo rpm -i ~/rpmbuild/RPMS/x86_64/libzuptsdk-2.0.0-*.rpm
```

### From source

```bash
tar -xzf libzuptsdk-2.0.0.tar.gz
cd libzuptsdk-2.0.0
make
sudo make install                  # → /usr/local/lib + /usr/local/include
sudo make install PREFIX=/opt/zupt # custom prefix
```

Verify the install:

```bash
pkg-config --modversion zuptsdk    # → 2.0.0
echo '#include <zuptsdk.h>
int main(){printf("%s\n",zuptsdk_version_string());}' \
  | cc -x c - $(pkg-config --cflags --libs zuptsdk) -o /tmp/v && /tmp/v
```

---

# Tutorial: a 60-second tour

This 5-step tutorial covers public-key encryption, password mode, field encryption, file encryption, and tamper detection. **Every example below is shipped as runnable code in [`doc/examples/`](doc/examples/) and verified by CI on every commit.**

## Step 1 — Generate a keypair

A keypair is two files: a public key (`alice.pub`, share with anyone) and a private key (`alice.priv`, keep secret).

```c
#include <zuptsdk_easy.h>

if (zuptsdk_easy_keygen("alice.pub", "alice.priv") != 0) {
    /* error — see zuptsdk_strerror() */
}
```

## Step 2 — Encrypt for a recipient

```c
const char *plaintext = "Top secret message";
uint8_t *ciphertext = NULL;
size_t  ct_len = 0;

zuptsdk_easy_encrypt("alice.pub",
                      (const uint8_t *)plaintext, strlen(plaintext),
                      &ciphertext, &ct_len);
/* … send ciphertext (ct_len bytes) to Alice … */
free(ciphertext);   /* release the heap buffer */
```

## Step 3 — Alice decrypts with her private key

```c
uint8_t *recovered = NULL;
size_t  rec_len = 0;

int rc = zuptsdk_easy_decrypt("alice.priv",
                               ciphertext, ct_len,
                               &recovered, &rec_len);
if (rc != 0) {
    /* MAC failed → ciphertext was tampered, OR wrong key, OR wrong format */
    fprintf(stderr, "decrypt failed: %s\n", zuptsdk_strerror(rc));
    return 1;
}
/* Use recovered[0 .. rec_len-1] */
free(recovered);
```

## Step 4 — Password mode (no keypair needed)

When you only have a password (e.g., user passphrase, terminal prompt):

```c
const char *password = "correct horse battery staple";
const char *secret = "Bank account: 12345";

uint8_t *blob = NULL; size_t blob_sz = 0;
zuptsdk_easy_encrypt_password(password,
    (const uint8_t *)secret, strlen(secret),
    &blob, &blob_sz);

/* … later … */
uint8_t *plain = NULL; size_t plain_sz = 0;
zuptsdk_easy_decrypt_password(password, blob, blob_sz, &plain, &plain_sz);
```

The KDF is Argon2id (memory-hard, RFC 9106 IETF-recommended parameters: 64 MB, t=3, p=1). One encrypt or decrypt call takes ~250 ms on a modern desktop.

## Step 5 — Field-level encryption (DB columns / JSON)

Derive a 32-byte key once, encrypt many small fields:

```c
/* Once at startup: */
uint8_t salt[16], master_key[32];
zuptsdk_easy_random_salt(salt);                     /* persist this */
zuptsdk_easy_derive_key("master pw", salt, master_key);

/* For each field: */
char *email_ct = NULL;
zuptsdk_easy_encrypt_field(master_key, "alice@example.com", &email_ct);
/* email_ct is base64; safe to store in a TEXT column */
char *email_pt = NULL;
zuptsdk_easy_decrypt_field(master_key, email_ct, &email_pt);

free(email_ct); free(email_pt);
zuptsdk_secure_zero(master_key, sizeof(master_key));
```

---

# Language bindings

All bindings are tested against the canonical `libzuptsdk.so.2.0.0` and live in [`bindings/`](bindings/).

## Python

```python
import zuptsdk

# Generate keypair
zuptsdk.keygen("alice.pub", "alice.priv")

# Encrypt for Alice
blob = zuptsdk.encrypt("alice.pub", b"Hello, post-quantum world!")

# Alice decrypts
plain = zuptsdk.decrypt("alice.priv", blob)
assert plain == b"Hello, post-quantum world!"

# Password mode
blob = zuptsdk.encrypt_password("strong passphrase", b"secret data")
plain = zuptsdk.decrypt_password("strong passphrase", blob)

# File mode (no size limit)
zuptsdk.encrypt_file("alice.pub", "doc.pdf", "doc.pdf.enc")
zuptsdk.decrypt_file("alice.priv", "doc.pdf.enc", "doc-recovered.pdf")

# Tamper detection — raises ZuptError
try:
    blob = bytearray(zuptsdk.encrypt("alice.pub", b"secret"))
    blob[10] ^= 1
    zuptsdk.decrypt("alice.priv", bytes(blob))
except zuptsdk.ZuptError as e:
    print(f"Caught tampering: code={e.code}, message={e.message}")
```

**Install**: copy [`bindings/python/zuptsdk.py`](bindings/python/zuptsdk.py) into your project. Pure ctypes, zero pip dependencies. Tested with Python 3.8+.

**Test suite**: 13 properties, all passing. Run with:

```bash
PYTHONPATH=bindings/python python3 tests/test_python.py
```

## Node.js

```javascript
const zupt = require('zuptsdk');

// Generate keypair
zupt.keygen('alice.pub', 'alice.priv');

// Encrypt
const blob = zupt.encrypt('alice.pub', Buffer.from('Hello!'));

// Decrypt
const plain = zupt.decrypt('alice.priv', blob);
console.log(plain.toString());  // "Hello!"

// Password mode
const blob2 = zupt.encryptPassword('passphrase', Buffer.from('secret'));
const plain2 = zupt.decryptPassword('passphrase', blob2);

// Field encryption
const salt = zupt.randomSalt();
const key = zupt.deriveKey('master', salt);
const ct = zupt.encryptField(key, 'alice@example.com');
const pt = zupt.decryptField(key, ct);
```

**Install**: `npm install koffi` then copy [`bindings/node/zuptsdk.js`](bindings/node/zuptsdk.js).

## Go

```go
package main

import (
    "fmt"
    "log"

    zupt "github.com/cristiancmoises/libzuptsdk/bindings/go"
)

func main() {
    if err := zupt.Keygen("alice.pub", "alice.priv"); err != nil {
        log.Fatal(err)
    }

    blob, err := zupt.Encrypt("alice.pub", []byte("Hello"))
    if err != nil {
        log.Fatal(err)
    }

    plain, err := zupt.Decrypt("alice.priv", blob)
    if err != nil {
        log.Fatal(err)
    }
    fmt.Printf("%s\n", plain)
}
```

**Build**:

```bash
go build  # cgo automatically links against libzuptsdk via pkg-config
```

## Rust

`Cargo.toml`:

```toml
[dependencies]
zuptsdk = "2.0"
```

`src/main.rs`:

```rust
use zuptsdk::{keygen, encrypt, decrypt};

fn main() -> Result<(), zuptsdk::Error> {
    keygen("alice.pub", "alice.priv")?;

    let blob = encrypt("alice.pub", b"Hello, Rust!")?;
    let plain = decrypt("alice.priv", &blob)?;

    assert_eq!(plain, b"Hello, Rust!");
    Ok(())
}
```

The Rust crate uses `pkg-config` via `build.rs` to locate libzuptsdk. Install the `libzuptsdk-dev` package or run `make install` first.

## C++

`zuptsdk.hpp` ships a thin RAII wrapper:

```cpp
#include <zuptsdk_easy.h>
#include <vector>
#include <stdexcept>

class ZuptError : public std::runtime_error {
public:
    int code;
    ZuptError(int rc, const std::string& op)
      : std::runtime_error(op + ": " + zuptsdk_strerror(rc)), code(rc) {}
};

std::vector<uint8_t> encrypt_for(const std::string& pubkey,
                                  const std::vector<uint8_t>& plaintext) {
    uint8_t* blob = nullptr;
    size_t blob_sz = 0;
    int rc = zuptsdk_easy_encrypt(pubkey.c_str(),
                                   plaintext.data(), plaintext.size(),
                                   &blob, &blob_sz);
    if (rc != 0) throw ZuptError(rc, "encrypt");
    std::vector<uint8_t> out(blob, blob + blob_sz);
    zuptsdk_free(blob);
    return out;
}
```

Compile with:

```bash
g++ -std=c++17 myapp.cpp $(pkg-config --cflags --libs zuptsdk) -o myapp
```

## C (low-level)

```c
#include <zuptsdk.h>
#include <zuptsdk_easy.h>

int main(void) {
    /* see Step 1-5 tutorial above; full example in
       doc/examples/01_pq_keypair.c */
}
```

Compile:

```bash
cc myapp.c $(pkg-config --cflags --libs zuptsdk) -o myapp
```

---

# API reference

## Public API: convenience layer (`zuptsdk_easy.h`)

Recommended for **application developers**. The `easy_*` API hides ABI complexity behind 11 simple functions.

| Function | Purpose |
|---|---|
| `zuptsdk_easy_keygen(pub, priv)` | Generate a hybrid keypair to disk |
| `zuptsdk_easy_encrypt(pub, pt, len, &blob, &sz)` | PQ encrypt for recipient |
| `zuptsdk_easy_decrypt(priv, blob, sz, &pt, &len)` | PQ decrypt with private key |
| `zuptsdk_easy_encrypt_password(pw, pt, len, &blob, &sz)` | Password-based encrypt (Argon2id) |
| `zuptsdk_easy_decrypt_password(pw, blob, sz, &pt, &len)` | Password-based decrypt |
| `zuptsdk_easy_encrypt_file(pub, in, out, cb, ud)` | File encrypt (streamed) |
| `zuptsdk_easy_decrypt_file(priv, in, out, cb, ud)` | File decrypt (streamed) |
| `zuptsdk_easy_encrypt_field(key, str, &b64)` | Encrypt small string → base64 |
| `zuptsdk_easy_decrypt_field(key, b64, &str)` | Decrypt base64 → string |
| `zuptsdk_easy_derive_key(pw, salt, &key)` | Argon2id KDF (32-byte key) |
| `zuptsdk_easy_random_salt(&salt)` | OS-CSPRNG 16-byte salt |

## Public API: lifecycle (`zuptsdk.h`)

For applications needing fine-grained control (custom thread count, progress callbacks, secure-buffer keys):

```c
zuptsdk_ctx_t  *ctx; zuptsdk_ctx_create(&ctx);
zuptsdk_ctx_set_threads(ctx, 8);
/* ... use ctx ... */
zuptsdk_ctx_destroy(ctx);
```

See [`include/zuptsdk.h`](include/zuptsdk.h) for the full ABI.

## Error handling

All functions return `0` on success, non-zero on failure. Use `zuptsdk_strerror(rc)` to convert to a printable message. Common codes:

| Code | Meaning |
|---|---|
| 0 | success |
| -1 | generic error (see `zuptsdk_last_error_detail()`) |
| -2 | invalid argument |
| -3 | I/O error (file not found, permission denied) |
| -4 | out of memory |
| -10 | authentication failure (MAC reject — tampered or wrong key) |
| -11 | format error (not a libzuptsdk blob, or version too new) |

The authoritative code list is `zuptsdk_error_t` in [`include/zuptsdk.h`](include/zuptsdk.h). Note there is **no** distinct "KEM decapsulation failure" code: ML-KEM-768 uses FIPS 203 implicit rejection, so a wrong key or tampered ciphertext always surfaces as the authentication/MAC failure above, never as a KEM-level error.

---

# Cryptographic primitives

| Layer | Algorithm | Reference |
|---|---|---|
| KEM | ML-KEM-768 | FIPS 203 |
| ECDH | X25519 | RFC 7748 |
| Combiner | HKDF-SHA3-256 | RFC 5869 + FIPS 202 |
| Key commitment | BLAKE2b-MAC | Bellare-Hoang 2022 |
| AEAD (default) | XChaCha20-Poly1305 | RFC 8439 + draft-irtf-cfrg-xchacha |
| AEAD (NMR) | AES-256-SIV | RFC 5297 |
| Password KDF | Argon2id | RFC 9106 |
| Hash | SHA-256, SHA3-256, BLAKE2b | FIPS 180-4, FIPS 202, RFC 7693 |

See [`SECURITY.md`](SECURITY.md) for the full threat model and protocol diagrams.

---

# ML-KEM-768 conformance status

The ML-KEM-768 implementation in `src/zupt_mlkem.c` is verified conformant to
**FIPS 203** by the [`conformance-suite/`](conformance-suite/) gate, wired into
CI at [`.forgejo/workflows/mlkem-conformance.yaml`](.forgejo/workflows/mlkem-conformance.yaml):

| Check | Result |
|---|---|
| Official NIST ACVP vectors (keyGen, encaps, decaps, §7.2/§7.3 key checks) | **80/80** |
| Differential vs kyber-py 1.2.0 (both directions) | **100/100 ×2** |
| Differential vs RustCrypto `ml-kem` 0.2.3 (both directions) | **50/50 ×2** |
| Self-roundtrip / tampered-ciphertext rejection | **1000/1000 / 1000/1000** |
| Constant-time (dudect + ctgrind) | clean — see [`CT_VERIFICATION.md`](CT_VERIFICATION.md) |

Run it yourself:

```bash
gcc -O2 -Iinclude -Isrc conformance-suite/kat_mlkem768_acvp.c \
    src/zupt_mlkem.c src/zupt_keccak.c src/zupt_sha256.c -o katz
cd conformance-suite && python3 run_kats.py ../katz     # expect 80/80
```

> **Two important caveats.**
> 1. **This conformance holds for the from-source library.** The canonical
>    `prebuilt/libzuptsdk.so.2.0.0` predates the 2026-07-02 fix
>    (see [`MLKEM_CONFORMANCE_FIX.md`](MLKEM_CONFORMANCE_FIX.md)) and is **not**
>    rebuilt from this patched source — it must be regenerated and re-audited
>    before it can be claimed conformant.
> 2. **Migration hazard.** An ML-KEM-hybrid archive or keypair produced by a
>    *pre-fix* build will **not** interoperate with a *post-fix* build: the
>    corrected KEM derives a different (now-standard) shared secret, so the AEAD
>    MAC fails and the archive reads as tampered/wrong-key. Re-encrypt affected
>    data with a post-fix build.

---

# Source vs prebuilt

This repository ships **two libraries**, an intentional design:

| Library | Source | Symbols | When to use |
|---|---|---|---|
| `libzuptsdk-base.so` | from source in this repo | 55 (ZUPTSDK_1.0 subset) | Verifying the build, embedded use, builds where binary blobs aren't allowed |
| `libzuptsdk.so` | bundled prebuilt | 68 (full ZUPTSDK_1.0 + 2.1, including `easy_*`) | **Production use; what `make install` installs and downstream apps link against** |

**Why ship a prebuilt at all?** The `easy_*` convenience layer (`zuptsdk_easy_encrypt`, etc.) and a few v2.1 functions don't have source code in this public tree for legacy reasons. The prebuilt binary is the audited, production-deployed artifact; the from-source library proves the rest of the codebase is buildable and a strict subset.

> ⚠️ **ML-KEM-768 conformance & the prebuilt.** The FIPS 203 conformance fix and its 80/80 ACVP verification apply to the **from-source** library. The prebuilt has not been rebuilt from the patched source; regenerate and re-audit it before relying on it for standards-conformant, interoperable ML-KEM. See [ML-KEM-768 conformance status](#ml-kem-768-conformance-status).

The `make audit` target verifies on every build that:

1. Architecture matches between source and prebuilt
2. SONAMEs follow the stable convention
3. The 24 symbols common to both are byte-compatible
4. All canonical-only symbols are in the public `zuptsdk_*` namespace
5. Source build does not leak any private symbols

**Roadmap**: open-source the missing `easy_*` source so the from-source library is a complete drop-in replacement. Tracked as the top open item.

---

# Build, test, audit

```
make             Build from-source + stage canonical prebuilt
make test        Compile + run smoke test, then audit, then license check
make audit       Verify source build is a strict subset of canonical
make audit-licenses  Verify all files have AGPL SPDX header
make test-asan   Build from-source with ASAN/UBSAN
make install     Install canonical + headers + pkg-config
make uninstall   Remove installed files
make dist        Build deterministic source tarball
make clean       Remove build artifacts
```

Test suite — **30/30 properties pass**:

- 10 smoke tests against canonical (version, keygen, encrypt/decrypt roundtrip, tamper, wrong-key, password mode)
- 13 audit tests (architecture, SONAME, ABI versions, namespace hygiene, no-leakage)
- 6 ASAN/UBSAN smoke tests against the source-build (ctx/options lifecycle, secure_zero)
- 1 license audit (every file has AGPL SPDX)

Plus 13 Python binding tests, 12 Node.js binding tests, 4 Go binding tests, and 5 Rust binding tests — all working code in `bindings/`.

Separately, the **ML-KEM-768 conformance gate** ([`conformance-suite/`](conformance-suite/)) verifies FIPS 203 compliance of the from-source KEM: **80/80** official NIST ACVP vectors, byte-for-byte differentials against two independent implementations (both directions), and a dudect + ctgrind constant-time check. See [ML-KEM-768 conformance status](#ml-kem-768-conformance-status).

---

# Versioning & ABI commitment

`libzuptsdk` follows strict ABI versioning at the linker level:

- **Major** (`libzuptsdk.so.2`): incompatible ABI break
- **Minor** (`ZUPTSDK_2.1` block in `zuptsdk.map`): additive, backward-compatible
- **Patch** (`libzuptsdk.so.2.0.0` → `2.0.1`): bug fixes only, no API change

The contract:

- **No symbol in `ZUPTSDK_1.0` will ever be removed or change behavior.**
- **No symbol in `ZUPTSDK_1.0` will change its function signature.**
- **New symbols added in 2.x go into `ZUPTSDK_2.x` blocks.** Old code linked against `ZUPTSDK_1.0` keeps working.
- Any incompatible change is a `libzuptsdk.so.3` event (major SONAME bump, separate parallel-installable library).

Current: **2.0.0** (ABI: ZUPTSDK_1.0 + ZUPTSDK_2.1).

---

# Security

See [`SECURITY.md`](SECURITY.md) for the full threat model, cryptographic construction details, side-channel mitigations, and known limitations.

**Reporting vulnerabilities**: email **`zupt@riseup.net`**. Do not file public GitHub issues for security bugs. PGP key: TBD.

**Audited**:

- 169 unit tests (compile + roundtrip + tamper detection)
- 750,000 mutation fuzz iterations under ASAN/UBSAN
- Constant-time crypto primitives verified by Jasmin (jasminc 2026.03.0)
- Symbol versioning, no internal namespace leakage
- Full report: [`AUDIT.md`](AUDIT.md)

**Not audited** (open items):

- External independent crypto audit (cost, not engineering — ~$30-60k)
- The `zuptsdk_easy_*` source — only the binary has been audited
- **The canonical prebuilt has not been rebuilt from the post-2026-07-02
  ML-KEM source**; its ML-KEM conformance is not yet re-verified (the
  from-source library is — 80/80 ACVP). See [ML-KEM-768 conformance status](#ml-kem-768-conformance-status).

---

# License

This project is licensed under the **GNU Affero General Public License version 3 or later** (AGPL-3.0-or-later). See [`LICENSE`](LICENSE).

Every source file carries `SPDX-License-Identifier: AGPL-3.0-or-later`. The `make audit-licenses` target enforces this on every CI run.

For commercial licensing inquiries: `zupt@riseup.net`.

---

# See also

- [zupt](https://github.com/cristiancmoises/zupt) — CLI + GUI built on libzuptsdk
- [zupt SECURITY.md](https://github.com/cristiancmoises/zupt/blob/main/SECURITY.md) — threat model for the CLI
- [Jasmin](https://github.com/jasmin-lang/jasmin) — verified-CT cryptography compiler used here
- [NIST FIPS 203](https://csrc.nist.gov/pubs/fips/203/final) — ML-KEM specification

---

**libzuptsdk 2.0.0** · Author: Cristian Cezar Moisés · License: AGPL-3.0-or-later
