#!/usr/bin/env node
/**
 * vuptsdk Node.js binding test suite
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Cristian Cezar Moisés
 *
 * Run with:
 *     ZUPTSDK_LIBRARY=/path/to/libvuptsdk.so.2.0.0 \
 *     node tests/test_node.js
 *
 * Requires: npm install koffi
 */
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');

// Resolve bindings path
const bindingsPath = path.join(__dirname, '..', 'bindings', 'node', 'vuptsdk.js');
const zupt = require(bindingsPath);

let pass = 0, fail = 0;

function test(name, fn) {
    process.stderr.write(`  ${name.padEnd(55)}`);
    try {
        fn();
        process.stderr.write('PASS\n');
        pass++;
    } catch (e) {
        process.stderr.write(`FAIL (${e.message})\n`);
        fail++;
    }
}

console.log('═'.repeat(60));
console.log(`  vuptsdk Node.js bindings test — version ${zupt.version()}`);
console.log('═'.repeat(60));

const tmpDir = fs.mkdtempSync(path.join(os.tmpdir(), 'zupt-test-'));

test('version returns string', () => {
    const v = zupt.version();
    if (typeof v !== 'string' || v.length < 3) throw new Error(`got ${v}`);
});

test('keygen creates pub + priv files', () => {
    const pub = path.join(tmpDir, 'k.pub');
    const priv = path.join(tmpDir, 'k.priv');
    zupt.keygen(pub, priv);
    if (!fs.existsSync(pub) || fs.statSync(pub).size === 0) throw new Error('pub empty');
    if (!fs.existsSync(priv) || fs.statSync(priv).size === 0) throw new Error('priv empty');
});

test('encrypt / decrypt roundtrip', () => {
    const pub = path.join(tmpDir, 'rt.pub');
    const priv = path.join(tmpDir, 'rt.priv');
    zupt.keygen(pub, priv);
    const msg = Buffer.from('Hello, post-quantum world!');
    const blob = zupt.encrypt(pub, msg);
    if (blob.length <= msg.length) throw new Error('blob too small');
    const recovered = zupt.decrypt(priv, blob);
    if (!recovered.equals(msg)) throw new Error(`got ${recovered.toString()}`);
});

test('decrypt rejects tampered blob', () => {
    const pub = path.join(tmpDir, 't.pub');
    const priv = path.join(tmpDir, 't.priv');
    zupt.keygen(pub, priv);
    const blob = Buffer.from(zupt.encrypt(pub, Buffer.from('secret')));
    blob[Math.floor(blob.length / 2)] ^= 0x01;
    try {
        zupt.decrypt(priv, blob);
        throw new Error('should have failed');
    } catch (e) {
        if (e.name !== 'ZuptError') throw e;
    }
});

test('decrypt with wrong key fails', () => {
    const pubA = path.join(tmpDir, 'a.pub');
    const privA = path.join(tmpDir, 'a.priv');
    const pubB = path.join(tmpDir, 'b.pub');
    const privB = path.join(tmpDir, 'b.priv');
    zupt.keygen(pubA, privA);
    zupt.keygen(pubB, privB);
    const blob = zupt.encrypt(pubA, Buffer.from('for Alice'));
    try {
        zupt.decrypt(privB, blob);
        throw new Error('should have failed');
    } catch (e) {
        if (e.name !== 'ZuptError') throw e;
    }
});

test('encryptPassword / decryptPassword roundtrip', () => {
    const blob = zupt.encryptPassword('correct horse battery staple',
        Buffer.from('top secret'));
    const recovered = zupt.decryptPassword('correct horse battery staple', blob);
    if (recovered.toString() !== 'top secret') throw new Error('mismatch');
});

test('decryptPassword rejects wrong password', () => {
    const blob = zupt.encryptPassword('right', Buffer.from('hello'));
    try {
        zupt.decryptPassword('wrong', blob);
        throw new Error('should have failed');
    } catch (e) {
        if (e.name !== 'ZuptError') throw e;
    }
});

test('encryptFile / decryptFile roundtrip', () => {
    const pub = path.join(tmpDir, 'f.pub');
    const priv = path.join(tmpDir, 'f.priv');
    zupt.keygen(pub, priv);
    const inPath = path.join(tmpDir, 'doc.bin');
    const encPath = path.join(tmpDir, 'doc.enc');
    const outPath = path.join(tmpDir, 'doc-out.bin');
    const data = crypto.randomBytes(50_000);
    fs.writeFileSync(inPath, data);
    zupt.encryptFile(pub, inPath, encPath);
    if (!fs.existsSync(encPath)) throw new Error('no enc file');
    zupt.decryptFile(priv, encPath, outPath);
    const recovered = fs.readFileSync(outPath);
    if (!recovered.equals(data)) throw new Error('mismatch');
});

test('deriveKey + encryptField / decryptField', () => {
    const salt = zupt.randomSalt();
    if (salt.length !== 16) throw new Error(`salt is ${salt.length} bytes`);
    const key = zupt.deriveKey('master', salt);
    if (key.length !== 32) throw new Error(`key is ${key.length} bytes`);

    const ct = zupt.encryptField(key, 'user@example.com');
    if (typeof ct !== 'string') throw new Error('ct not string');
    const pt = zupt.decryptField(key, ct);
    if (pt !== 'user@example.com') throw new Error(`got ${pt}`);
});

test('randomSalt produces distinct values', () => {
    const a = zupt.randomSalt();
    const b = zupt.randomSalt();
    if (a.equals(b)) throw new Error('identical salts');
});

test('ZuptError carries code', () => {
    try {
        zupt.decrypt('/nonexistent', Buffer.alloc(32));
        throw new Error('should have raised');
    } catch (e) {
        if (e.name !== 'ZuptError') throw e;
        if (typeof e.code !== 'number') throw new Error('no code');
        if (e.code === 0) throw new Error('code is zero');
    }
});

test('large buffer (1 MB) roundtrip', () => {
    const big = crypto.randomBytes(1024 * 1024);
    const blob = zupt.encryptPassword('test', big);
    const recovered = zupt.decryptPassword('test', blob);
    if (!recovered.equals(big)) throw new Error('mismatch');
});

// Cleanup
fs.rmSync(tmpDir, { recursive: true, force: true });

console.log();
console.log('  ─────────────────────────────────');
console.log(`  Node.js bindings: ${pass} passed, ${fail} failed`);
console.log('  ─────────────────────────────────');

process.exit(fail === 0 ? 0 : 1);
