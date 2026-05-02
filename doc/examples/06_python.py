#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""All Python examples from README — must run end-to-end."""
import os
import sys
import tempfile

sys.path.insert(0, "/home/claude/libzuptsdk/bindings/python")
import zuptsdk

# Example 1: keygen + roundtrip
with tempfile.TemporaryDirectory() as d:
    pub = os.path.join(d, "alice.pub")
    priv = os.path.join(d, "alice.priv")
    zuptsdk.keygen(pub, priv)
    blob = zuptsdk.encrypt(pub, b"Hello, Alice!")
    plain = zuptsdk.decrypt(priv, blob)
    assert plain == b"Hello, Alice!", f"Example 1 failed: {plain!r}"
    print("Example 1 (keygen + roundtrip):  PASS")

# Example 2: password mode
blob = zuptsdk.encrypt_password("strong passphrase", b"secret data")
plain = zuptsdk.decrypt_password("strong passphrase", blob)
assert plain == b"secret data"
print("Example 2 (password mode):       PASS")

# Example 3: field encryption (DB column scenario)
salt = zuptsdk.random_salt()
key = zuptsdk.derive_key("master", salt)
ct = zuptsdk.encrypt_field(key, "alice@example.com")
pt = zuptsdk.decrypt_field(key, ct)
assert pt == "alice@example.com"
print("Example 3 (field encryption):    PASS")

# Example 4: file mode
with tempfile.TemporaryDirectory() as d:
    pub = os.path.join(d, "k.pub")
    priv = os.path.join(d, "k.priv")
    zuptsdk.keygen(pub, priv)

    inp = os.path.join(d, "doc.txt")
    enc = os.path.join(d, "doc.enc")
    out = os.path.join(d, "doc-out.txt")
    with open(inp, "wb") as f:
        f.write(b"file content")

    zuptsdk.encrypt_file(pub, inp, enc)
    zuptsdk.decrypt_file(priv, enc, out)
    with open(out, "rb") as f:
        assert f.read() == b"file content"
print("Example 4 (file mode):           PASS")

# Example 5: tamper detection
with tempfile.TemporaryDirectory() as d:
    pub = os.path.join(d, "k.pub")
    priv = os.path.join(d, "k.priv")
    zuptsdk.keygen(pub, priv)
    blob = bytearray(zuptsdk.encrypt(pub, b"secret"))
    blob[len(blob) // 2] ^= 1
    try:
        zuptsdk.decrypt(priv, bytes(blob))
        assert False, "tamper not detected"
    except zuptsdk.ZuptError as e:
        assert e.code != 0
print("Example 5 (tamper detection):    PASS")

print()
print("All 5 README Python examples validated end-to-end.")
