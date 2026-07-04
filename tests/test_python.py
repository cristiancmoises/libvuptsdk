#!/usr/bin/env python3
"""
vuptsdk Python binding test suite
SPDX-License-Identifier: AGPL-3.0-or-later
Copyright (c) 2026 Cristian Cezar Moisés

Run with:
    ZUPTSDK_LIBRARY=/path/to/libvuptsdk.so.2.0.0 \
    PYTHONPATH=bindings/python \
    python3 tests/test_python.py
"""
import os
import sys
import tempfile
import traceback

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bindings", "python"))
import vuptsdk

PASS = 0
FAIL = 0


def test(name):
    """Decorator: register a test function."""
    def wrap(fn):
        global PASS, FAIL
        sys.stderr.write(f"  {name:<55}")
        sys.stderr.flush()
        try:
            fn()
            sys.stderr.write("PASS\n")
            PASS += 1
        except AssertionError as e:
            sys.stderr.write(f"FAIL ({e})\n")
            FAIL += 1
        except Exception as e:
            sys.stderr.write(f"ERROR ({type(e).__name__}: {e})\n")
            traceback.print_exc(file=sys.stderr)
            FAIL += 1
        return fn
    return wrap


print("═" * 60)
print(f"  vuptsdk Python bindings test — version {vuptsdk.version()}")
print("═" * 60)


@test("version() returns string")
def _():
    v = vuptsdk.version()
    assert isinstance(v, str) and len(v) >= 3, f"got {v!r}"


@test("keygen creates pub + priv files")
def _():
    with tempfile.TemporaryDirectory() as d:
        pub = os.path.join(d, "k.pub")
        priv = os.path.join(d, "k.priv")
        vuptsdk.keygen(pub, priv)
        assert os.path.exists(pub) and os.path.getsize(pub) > 0
        assert os.path.exists(priv) and os.path.getsize(priv) > 0


@test("encrypt / decrypt roundtrip")
def _():
    with tempfile.TemporaryDirectory() as d:
        pub = os.path.join(d, "k.pub")
        priv = os.path.join(d, "k.priv")
        vuptsdk.keygen(pub, priv)
        msg = b"The quick brown fox jumps over the lazy dog."
        blob = vuptsdk.encrypt(pub, msg)
        assert isinstance(blob, bytes) and len(blob) > len(msg)
        recovered = vuptsdk.decrypt(priv, blob)
        assert recovered == msg, f"got {recovered!r}"


@test("decrypt rejects tampered blob")
def _():
    with tempfile.TemporaryDirectory() as d:
        pub = os.path.join(d, "k.pub")
        priv = os.path.join(d, "k.priv")
        vuptsdk.keygen(pub, priv)
        blob = bytearray(vuptsdk.encrypt(pub, b"secret"))
        # Flip a bit in the middle of the blob
        blob[len(blob) // 2] ^= 0x01
        try:
            vuptsdk.decrypt(priv, bytes(blob))
            assert False, "should have raised ZuptError"
        except vuptsdk.ZuptError:
            pass


@test("decrypt with wrong key fails")
def _():
    with tempfile.TemporaryDirectory() as d:
        pubA = os.path.join(d, "a.pub")
        privA = os.path.join(d, "a.priv")
        pubB = os.path.join(d, "b.pub")
        privB = os.path.join(d, "b.priv")
        vuptsdk.keygen(pubA, privA)
        vuptsdk.keygen(pubB, privB)
        blob = vuptsdk.encrypt(pubA, b"for Alice only")
        try:
            vuptsdk.decrypt(privB, blob)
            assert False, "should have raised"
        except vuptsdk.ZuptError:
            pass


@test("encrypt_password / decrypt_password roundtrip")
def _():
    pw = "correct horse battery staple"
    msg = b"top secret message"
    blob = vuptsdk.encrypt_password(pw, msg)
    recovered = vuptsdk.decrypt_password(pw, blob)
    assert recovered == msg


@test("decrypt_password rejects wrong password")
def _():
    blob = vuptsdk.encrypt_password("right", b"hello")
    try:
        vuptsdk.decrypt_password("wrong", blob)
        assert False, "should have raised"
    except vuptsdk.ZuptError:
        pass


@test("encrypt_file / decrypt_file roundtrip")
def _():
    with tempfile.TemporaryDirectory() as d:
        pub = os.path.join(d, "k.pub")
        priv = os.path.join(d, "k.priv")
        vuptsdk.keygen(pub, priv)

        plain_path = os.path.join(d, "doc.txt")
        enc_path = os.path.join(d, "doc.enc")
        rec_path = os.path.join(d, "doc-out.txt")
        original = b"file content " * 1000  # ~13 KB
        with open(plain_path, "wb") as f:
            f.write(original)

        vuptsdk.encrypt_file(pub, plain_path, enc_path)
        assert os.path.exists(enc_path) and os.path.getsize(enc_path) > 0

        vuptsdk.decrypt_file(priv, enc_path, rec_path)
        with open(rec_path, "rb") as f:
            recovered = f.read()
        assert recovered == original


@test("derive_key + encrypt_field / decrypt_field")
def _():
    salt = vuptsdk.random_salt()
    assert len(salt) == 16
    key = vuptsdk.derive_key("master password", salt)
    assert len(key) == 32

    ct = vuptsdk.encrypt_field(key, "user@example.com")
    pt = vuptsdk.decrypt_field(key, ct)
    assert pt == "user@example.com"


@test("random_salt produces 16 distinct bytes")
def _():
    s1 = vuptsdk.random_salt()
    s2 = vuptsdk.random_salt()
    assert len(s1) == 16 and len(s2) == 16
    assert s1 != s2, "salts should differ"


@test("ZuptError carries code and message")
def _():
    try:
        vuptsdk.decrypt("/nonexistent/key", b"\x00" * 32)
        assert False, "should have raised"
    except vuptsdk.ZuptError as e:
        assert isinstance(e.code, int)
        assert isinstance(e.message, str)
        assert e.code != 0


@test("unicode plaintext survives roundtrip")
def _():
    pw = "senha forte"
    # Mix of scripts including emoji
    msg = "Olá 世界 🔐 — мир".encode("utf-8")
    blob = vuptsdk.encrypt_password(pw, msg)
    recovered = vuptsdk.decrypt_password(pw, blob)
    assert recovered == msg


@test("large buffer (1 MB) roundtrip")
def _():
    pw = "test"
    big = os.urandom(1024 * 1024)
    blob = vuptsdk.encrypt_password(pw, big)
    recovered = vuptsdk.decrypt_password(pw, blob)
    assert recovered == big


print()
print(f"  ─────────────────────────────────")
print(f"  Python bindings: {PASS} passed, {FAIL} failed")
print(f"  ─────────────────────────────────")

sys.exit(0 if FAIL == 0 else 1)
