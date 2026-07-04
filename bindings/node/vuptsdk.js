/**
 * vuptsdk — Node.js bindings for libvuptsdk
 *
 * Copyright (c) 2026 Cristian Cezar Moisés
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Uses koffi (https://www.npmjs.com/package/koffi) for FFI.
 * Install with: npm install koffi
 *
 * Quick start
 * -----------
 *     const zupt = require('./vuptsdk');
 *     zupt.keygen('alice.pub', 'alice.priv');
 *     const blob = zupt.encrypt('alice.pub', Buffer.from('Hello'));
 *     const plain = zupt.decrypt('alice.priv', blob);
 *     console.log(plain.toString()); // "Hello"
 *
 * Override library path:
 *     ZUPTSDK_LIBRARY=/path/to/libvuptsdk.so node app.js
 */
'use strict';

const koffi = require('koffi');

// ─── Library loading ──────────────────────────────────────────────
const libPath = process.env.ZUPTSDK_LIBRARY ||
    'libvuptsdk.so.2' ||
    koffi.util.findLibrary('vuptsdk');

const lib = koffi.load(libPath);

// ─── Type aliases ─────────────────────────────────────────────────
const u8ptr_ptr = koffi.pointer(koffi.pointer('uint8_t'));
const sizet_ptr = koffi.pointer('size_t');
const cstring_ptr = koffi.pointer('char*');

// ─── C function signatures ────────────────────────────────────────
const c_version_string = lib.func('const char *zuptsdk_version_string()');
const c_strerror = lib.func('const char *zuptsdk_strerror(int)');
const c_free = lib.func('void zuptsdk_free(void *)');
const c_secure_zero = lib.func('void zuptsdk_secure_zero(void *, size_t)');

const c_keygen = lib.func(
    'int zuptsdk_easy_keygen(const char *pub, const char *priv)');

const c_encrypt = lib.func(
    'int zuptsdk_easy_encrypt(const char *pub, const uint8_t *pt, size_t pt_sz, _Out_ uint8_t **blob, _Out_ size_t *blob_sz)');

const c_decrypt = lib.func(
    'int zuptsdk_easy_decrypt(const char *priv, const uint8_t *blob, size_t blob_sz, _Out_ uint8_t **pt, _Out_ size_t *pt_sz)');

const c_encrypt_password = lib.func(
    'int zuptsdk_easy_encrypt_password(const char *pw, const uint8_t *pt, size_t pt_sz, _Out_ uint8_t **blob, _Out_ size_t *blob_sz)');

const c_decrypt_password = lib.func(
    'int zuptsdk_easy_decrypt_password(const char *pw, const uint8_t *blob, size_t blob_sz, _Out_ uint8_t **pt, _Out_ size_t *pt_sz)');

const c_encrypt_file = lib.func(
    'int zuptsdk_easy_encrypt_file(const char *pub, const char *in_path, const char *out_path, void *cb, void *user)');

const c_decrypt_file = lib.func(
    'int zuptsdk_easy_decrypt_file(const char *priv, const char *in_path, const char *out_path, void *cb, void *user)');

const c_encrypt_field = lib.func(
    'int zuptsdk_easy_encrypt_field(const uint8_t *key, const char *pt, _Out_ char **b64)');

const c_decrypt_field = lib.func(
    'int zuptsdk_easy_decrypt_field(const uint8_t *key, const char *b64, _Out_ char **pt)');

const c_random_salt = lib.func(
    'int zuptsdk_easy_random_salt(_Out_ uint8_t out[16])');

const c_derive_key = lib.func(
    'int zuptsdk_easy_derive_key(const char *pw, const uint8_t salt[16], _Out_ uint8_t key_out[32])');


// ─── Errors ───────────────────────────────────────────────────────
class ZuptError extends Error {
    constructor(code, op = '') {
        const msg = c_strerror(code);
        const full = op
            ? `${op}: libvuptsdk error ${code}: ${msg}`
            : `libvuptsdk error ${code}: ${msg}`;
        super(full);
        this.name = 'ZuptError';
        this.code = code;
        this.message_only = msg;
    }
}

function check(rc, op) {
    if (rc !== 0) throw new ZuptError(rc, op);
}


// ─── Public API ───────────────────────────────────────────────────

function version() {
    return c_version_string();
}

function keygen(pubPath, privPath) {
    check(c_keygen(pubPath, privPath), 'keygen');
}

function encrypt(pubPath, plaintext) {
    if (!Buffer.isBuffer(plaintext)) {
        throw new TypeError('plaintext must be a Buffer');
    }
    const blobPP = [null];
    const blobSzP = [0n];
    const rc = c_encrypt(pubPath, plaintext, plaintext.length, blobPP, blobSzP);
    check(rc, 'encrypt');
    const sz = Number(blobSzP[0]);
    const out = Buffer.from(koffi.decode(blobPP[0], 'uint8_t', sz));
    c_free(blobPP[0]);
    return out;
}

function decrypt(privPath, blob) {
    if (!Buffer.isBuffer(blob)) {
        throw new TypeError('blob must be a Buffer');
    }
    const ptPP = [null];
    const ptSzP = [0n];
    const rc = c_decrypt(privPath, blob, blob.length, ptPP, ptSzP);
    check(rc, 'decrypt');
    const sz = Number(ptSzP[0]);
    const out = Buffer.from(koffi.decode(ptPP[0], 'uint8_t', sz));
    c_free(ptPP[0]);
    return out;
}

function encryptPassword(password, plaintext) {
    if (!Buffer.isBuffer(plaintext)) {
        throw new TypeError('plaintext must be a Buffer');
    }
    const blobPP = [null];
    const blobSzP = [0n];
    const rc = c_encrypt_password(password, plaintext, plaintext.length, blobPP, blobSzP);
    check(rc, 'encryptPassword');
    const sz = Number(blobSzP[0]);
    const out = Buffer.from(koffi.decode(blobPP[0], 'uint8_t', sz));
    c_free(blobPP[0]);
    return out;
}

function decryptPassword(password, blob) {
    if (!Buffer.isBuffer(blob)) {
        throw new TypeError('blob must be a Buffer');
    }
    const ptPP = [null];
    const ptSzP = [0n];
    const rc = c_decrypt_password(password, blob, blob.length, ptPP, ptSzP);
    check(rc, 'decryptPassword');
    const sz = Number(ptSzP[0]);
    const out = Buffer.from(koffi.decode(ptPP[0], 'uint8_t', sz));
    c_free(ptPP[0]);
    return out;
}

function encryptFile(pubPath, inputPath, outputPath) {
    check(c_encrypt_file(pubPath, inputPath, outputPath, null, null), 'encryptFile');
}

function decryptFile(privPath, inputPath, outputPath) {
    check(c_decrypt_file(privPath, inputPath, outputPath, null, null), 'decryptFile');
}

function encryptField(key, plaintext) {
    if (!Buffer.isBuffer(key) || key.length !== 32) {
        throw new TypeError('key must be a 32-byte Buffer');
    }
    const out = [null];
    check(c_encrypt_field(key, plaintext, out), 'encryptField');
    const result = koffi.decode(out[0], 'string');
    c_free(out[0]);
    return result;
}

function decryptField(key, b64) {
    if (!Buffer.isBuffer(key) || key.length !== 32) {
        throw new TypeError('key must be a 32-byte Buffer');
    }
    const out = [null];
    check(c_decrypt_field(key, b64, out), 'decryptField');
    const result = koffi.decode(out[0], 'string');
    c_free(out[0]);
    return result;
}

function randomSalt() {
    const buf = Buffer.alloc(16);
    check(c_random_salt(buf), 'randomSalt');
    return buf;
}

function deriveKey(password, salt) {
    if (!Buffer.isBuffer(salt) || salt.length !== 16) {
        throw new TypeError('salt must be a 16-byte Buffer');
    }
    const key = Buffer.alloc(32);
    check(c_derive_key(password, salt, key), 'deriveKey');
    return key;
}


module.exports = {
    version,
    keygen,
    encrypt, decrypt,
    encryptPassword, decryptPassword,
    encryptFile, decryptFile,
    encryptField, decryptField,
    randomSalt, deriveKey,
    ZuptError,
};
