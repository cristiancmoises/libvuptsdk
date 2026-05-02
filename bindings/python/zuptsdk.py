"""
zuptsdk — Python bindings for libzuptsdk

Copyright (c) 2026 Cristian Cezar Moisés
SPDX-License-Identifier: AGPL-3.0-or-later

Pure-ctypes bindings for the libzuptsdk easy_* convenience API.
No external Python dependencies (ctypes is in the stdlib).

Quick start
-----------
    import zuptsdk

    # Generate a public/private keypair
    zuptsdk.keygen("alice.pub", "alice.priv")

    # Encrypt a message for Alice
    blob = zuptsdk.encrypt("alice.pub", b"Hello, Alice!")

    # Alice decrypts with her private key
    plaintext = zuptsdk.decrypt("alice.priv", blob)
    assert plaintext == b"Hello, Alice!"

Password mode
-------------
    blob = zuptsdk.encrypt_password("my passphrase", b"secret bytes")
    plaintext = zuptsdk.decrypt_password("my passphrase", blob)
    assert plaintext == b"secret bytes"

File mode
---------
    zuptsdk.encrypt_file("alice.pub", "doc.pdf", "doc.pdf.enc")
    zuptsdk.decrypt_file("alice.priv", "doc.pdf.enc", "doc-recovered.pdf")

Exceptions
----------
All errors raise `zuptsdk.ZuptError` with the C-side error code and
message attached as `.code` and `.message` attributes.
"""

import ctypes
import ctypes.util
import os
from typing import Optional


# ─────────────────────────────────────────────────────────────────────
# Library loading
# ─────────────────────────────────────────────────────────────────────

def _load_library():
    """Locate and load libzuptsdk.so. Override path with ZUPTSDK_LIBRARY env var."""
    override = os.environ.get("ZUPTSDK_LIBRARY")
    if override:
        return ctypes.CDLL(override)

    # Try standard locations
    for name in ("libzuptsdk.so.2", "libzuptsdk.so", "zuptsdk"):
        try:
            return ctypes.CDLL(name)
        except OSError:
            continue

    # Try via ldconfig
    found = ctypes.util.find_library("zuptsdk")
    if found:
        return ctypes.CDLL(found)

    raise RuntimeError(
        "libzuptsdk shared library not found. "
        "Install via 'make install' or set ZUPTSDK_LIBRARY=/path/to/libzuptsdk.so"
    )


_lib = _load_library()


# ─────────────────────────────────────────────────────────────────────
# C function signatures
# ─────────────────────────────────────────────────────────────────────

_lib.zuptsdk_version_string.restype = ctypes.c_char_p
_lib.zuptsdk_version_string.argtypes = []

_lib.zuptsdk_strerror.restype = ctypes.c_char_p
_lib.zuptsdk_strerror.argtypes = [ctypes.c_int]

_lib.zuptsdk_secure_zero.restype = None
_lib.zuptsdk_secure_zero.argtypes = [ctypes.c_void_p, ctypes.c_size_t]

_lib.zuptsdk_free.restype = None
_lib.zuptsdk_free.argtypes = [ctypes.c_void_p]

_lib.zuptsdk_easy_keygen.restype = ctypes.c_int
_lib.zuptsdk_easy_keygen.argtypes = [ctypes.c_char_p, ctypes.c_char_p]

_lib.zuptsdk_easy_encrypt.restype = ctypes.c_int
_lib.zuptsdk_easy_encrypt.argtypes = [
    ctypes.c_char_p,                     # pubkey path
    ctypes.c_char_p, ctypes.c_size_t,    # plaintext, len
    ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),  # blob_out
    ctypes.POINTER(ctypes.c_size_t),     # blob_sz_out
]

_lib.zuptsdk_easy_decrypt.restype = ctypes.c_int
_lib.zuptsdk_easy_decrypt.argtypes = [
    ctypes.c_char_p,                     # privkey path
    ctypes.c_char_p, ctypes.c_size_t,    # blob, blob_sz
    ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),  # plaintext_out
    ctypes.POINTER(ctypes.c_size_t),     # plaintext_sz_out
]

_lib.zuptsdk_easy_encrypt_password.restype = ctypes.c_int
_lib.zuptsdk_easy_encrypt_password.argtypes = [
    ctypes.c_char_p,                     # password
    ctypes.c_char_p, ctypes.c_size_t,    # plaintext, len
    ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),
    ctypes.POINTER(ctypes.c_size_t),
]

_lib.zuptsdk_easy_decrypt_password.restype = ctypes.c_int
_lib.zuptsdk_easy_decrypt_password.argtypes = [
    ctypes.c_char_p,                     # password
    ctypes.c_char_p, ctypes.c_size_t,    # blob, blob_sz
    ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),
    ctypes.POINTER(ctypes.c_size_t),
]

_lib.zuptsdk_easy_encrypt_file.restype = ctypes.c_int
_lib.zuptsdk_easy_encrypt_file.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
    ctypes.c_void_p, ctypes.c_void_p,
]

_lib.zuptsdk_easy_decrypt_file.restype = ctypes.c_int
_lib.zuptsdk_easy_decrypt_file.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p,
    ctypes.c_void_p, ctypes.c_void_p,
]

_lib.zuptsdk_easy_encrypt_field.restype = ctypes.c_int
_lib.zuptsdk_easy_encrypt_field.argtypes = [
    ctypes.c_char_p,                     # 32-byte key
    ctypes.c_char_p,                     # plaintext (cstring)
    ctypes.POINTER(ctypes.c_char_p),     # b64_out
]

_lib.zuptsdk_easy_decrypt_field.restype = ctypes.c_int
_lib.zuptsdk_easy_decrypt_field.argtypes = [
    ctypes.c_char_p, ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
]

_lib.zuptsdk_easy_derive_key.restype = ctypes.c_int
_lib.zuptsdk_easy_derive_key.argtypes = [
    ctypes.c_char_p,                     # password
    ctypes.c_char_p,                     # salt[16]
    ctypes.c_char_p,                     # key_out[32]
]

_lib.zuptsdk_easy_random_salt.restype = ctypes.c_int
_lib.zuptsdk_easy_random_salt.argtypes = [ctypes.c_char_p]


# ─────────────────────────────────────────────────────────────────────
# Exceptions
# ─────────────────────────────────────────────────────────────────────

class ZuptError(Exception):
    """Raised on any libzuptsdk error."""

    def __init__(self, code: int, op: str = ""):
        self.code = code
        msg_bytes = _lib.zuptsdk_strerror(code) or b"unknown error"
        self.message = msg_bytes.decode("utf-8", errors="replace")
        full = f"libzuptsdk error {code}: {self.message}"
        if op:
            full = f"{op}: {full}"
        super().__init__(full)


def _check(rc: int, op: str = "") -> None:
    if rc != 0:
        raise ZuptError(rc, op)


# ─────────────────────────────────────────────────────────────────────
# Public Python API
# ─────────────────────────────────────────────────────────────────────

def version() -> str:
    """Return the runtime libzuptsdk version, e.g. '2.1.5'."""
    return _lib.zuptsdk_version_string().decode("utf-8")


def keygen(pubkey_path: str, privkey_path: str) -> None:
    """Generate a hybrid (ML-KEM-768 + X25519) keypair.

    Args:
        pubkey_path: where to write the public key file
        privkey_path: where to write the private key file (keep secret!)
    """
    rc = _lib.zuptsdk_easy_keygen(
        pubkey_path.encode("utf-8"),
        privkey_path.encode("utf-8"),
    )
    _check(rc, "keygen")


def encrypt(pubkey_path: str, plaintext: bytes) -> bytes:
    """Encrypt for the recipient identified by their public key file.

    Args:
        pubkey_path: path to the recipient's .pub file
        plaintext: bytes to encrypt
    Returns:
        ciphertext blob (bytes) — opaque, includes KEM ciphertext + AEAD
    """
    blob_p = ctypes.POINTER(ctypes.c_uint8)()
    blob_sz = ctypes.c_size_t(0)
    rc = _lib.zuptsdk_easy_encrypt(
        pubkey_path.encode("utf-8"),
        plaintext, len(plaintext),
        ctypes.byref(blob_p), ctypes.byref(blob_sz),
    )
    _check(rc, "encrypt")
    try:
        return ctypes.string_at(blob_p, blob_sz.value)
    finally:
        _lib.zuptsdk_free(blob_p)


def decrypt(privkey_path: str, blob: bytes) -> bytes:
    """Decrypt with the recipient's private key.

    Args:
        privkey_path: path to recipient's .priv file
        blob: ciphertext from encrypt()
    Returns:
        plaintext bytes
    Raises:
        ZuptError if blob is tampered, key is wrong, or format is invalid
    """
    pt_p = ctypes.POINTER(ctypes.c_uint8)()
    pt_sz = ctypes.c_size_t(0)
    rc = _lib.zuptsdk_easy_decrypt(
        privkey_path.encode("utf-8"),
        blob, len(blob),
        ctypes.byref(pt_p), ctypes.byref(pt_sz),
    )
    _check(rc, "decrypt")
    try:
        return ctypes.string_at(pt_p, pt_sz.value)
    finally:
        _lib.zuptsdk_free(pt_p)


def encrypt_password(password: str, plaintext: bytes) -> bytes:
    """Encrypt with a password (Argon2id KDF + AEAD).

    Slower than public-key encrypt due to Argon2id (~250 ms by default).
    """
    blob_p = ctypes.POINTER(ctypes.c_uint8)()
    blob_sz = ctypes.c_size_t(0)
    rc = _lib.zuptsdk_easy_encrypt_password(
        password.encode("utf-8"),
        plaintext, len(plaintext),
        ctypes.byref(blob_p), ctypes.byref(blob_sz),
    )
    _check(rc, "encrypt_password")
    try:
        return ctypes.string_at(blob_p, blob_sz.value)
    finally:
        _lib.zuptsdk_free(blob_p)


def decrypt_password(password: str, blob: bytes) -> bytes:
    """Decrypt a password-encrypted blob."""
    pt_p = ctypes.POINTER(ctypes.c_uint8)()
    pt_sz = ctypes.c_size_t(0)
    rc = _lib.zuptsdk_easy_decrypt_password(
        password.encode("utf-8"),
        blob, len(blob),
        ctypes.byref(pt_p), ctypes.byref(pt_sz),
    )
    _check(rc, "decrypt_password")
    try:
        return ctypes.string_at(pt_p, pt_sz.value)
    finally:
        _lib.zuptsdk_free(pt_p)


def encrypt_file(pubkey_path: str, input_path: str, output_path: str) -> None:
    """Encrypt a file. No size limit (streamed)."""
    rc = _lib.zuptsdk_easy_encrypt_file(
        pubkey_path.encode("utf-8"),
        input_path.encode("utf-8"),
        output_path.encode("utf-8"),
        None, None,
    )
    _check(rc, "encrypt_file")


def decrypt_file(privkey_path: str, input_path: str, output_path: str) -> None:
    """Decrypt a file."""
    rc = _lib.zuptsdk_easy_decrypt_file(
        privkey_path.encode("utf-8"),
        input_path.encode("utf-8"),
        output_path.encode("utf-8"),
        None, None,
    )
    _check(rc, "decrypt_file")


def encrypt_field(key: bytes, plaintext: str) -> str:
    """Field-level encryption: small string in, base64 out.

    For DB columns or JSON fields. Use a 32-byte key derived once via
    derive_key() and reuse for many fields.
    """
    if len(key) != 32:
        raise ValueError("key must be exactly 32 bytes")
    out = ctypes.c_char_p()
    rc = _lib.zuptsdk_easy_encrypt_field(
        key, plaintext.encode("utf-8"),
        ctypes.byref(out),
    )
    _check(rc, "encrypt_field")
    try:
        return out.value.decode("ascii")
    finally:
        _lib.zuptsdk_free(out)


def decrypt_field(key: bytes, b64: str) -> str:
    """Decrypt a field-encrypted string."""
    if len(key) != 32:
        raise ValueError("key must be exactly 32 bytes")
    out = ctypes.c_char_p()
    rc = _lib.zuptsdk_easy_decrypt_field(
        key, b64.encode("ascii"),
        ctypes.byref(out),
    )
    _check(rc, "decrypt_field")
    try:
        return out.value.decode("utf-8")
    finally:
        _lib.zuptsdk_free(out)


def random_salt() -> bytes:
    """Generate a 16-byte random salt suitable for derive_key()."""
    buf = ctypes.create_string_buffer(16)
    rc = _lib.zuptsdk_easy_random_salt(buf)
    _check(rc, "random_salt")
    return buf.raw[:16]


def derive_key(password: str, salt: bytes) -> bytes:
    """Derive a deterministic 32-byte key from a password via Argon2id.

    Use this once at startup to obtain a long-lived key for field encryption.
    """
    if len(salt) != 16:
        raise ValueError("salt must be exactly 16 bytes")
    out = ctypes.create_string_buffer(32)
    rc = _lib.zuptsdk_easy_derive_key(
        password.encode("utf-8"),
        salt,
        out,
    )
    _check(rc, "derive_key")
    return out.raw[:32]


def secure_zero(buf: bytearray) -> None:
    """Zero a bytearray's memory before it's freed."""
    if not isinstance(buf, bytearray):
        raise TypeError("secure_zero requires a bytearray (mutable)")
    addr = (ctypes.c_uint8 * len(buf)).from_buffer(buf)
    _lib.zuptsdk_secure_zero(addr, len(buf))


__all__ = [
    "version",
    "keygen",
    "encrypt", "decrypt",
    "encrypt_password", "decrypt_password",
    "encrypt_file", "decrypt_file",
    "encrypt_field", "decrypt_field",
    "random_salt", "derive_key",
    "secure_zero",
    "ZuptError",
]
