//! # zuptsdk — Rust bindings for libzuptsdk
//!
//! Copyright (c) 2026 Cristian Cezar Moisés
//! SPDX-License-Identifier: AGPL-3.0-or-later
//!
//! Post-quantum hybrid cryptography (ML-KEM-768 + X25519) with safe Rust API.
//!
//! ## Quick start
//!
//! ```no_run
//! use zuptsdk::{keygen, encrypt, decrypt};
//!
//! keygen("alice.pub", "alice.priv")?;
//!
//! let blob = encrypt("alice.pub", b"Hello, Alice!")?;
//! let plain = decrypt("alice.priv", &blob)?;
//! assert_eq!(plain, b"Hello, Alice!");
//! # Ok::<(), zuptsdk::Error>(())
//! ```
//!
//! ## Password mode
//!
//! ```no_run
//! use zuptsdk::{encrypt_password, decrypt_password};
//!
//! let blob = encrypt_password("strong passphrase", b"secret data")?;
//! let plain = decrypt_password("strong passphrase", &blob)?;
//! # Ok::<(), zuptsdk::Error>(())
//! ```

use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::path::Path;
use std::ptr;
use std::slice;

// ─── FFI declarations ─────────────────────────────────────────────
#[link(name = "zuptsdk")]
extern "C" {
    fn zuptsdk_version_string() -> *const c_char;
    fn zuptsdk_strerror(code: c_int) -> *const c_char;
    fn zuptsdk_free(ptr: *mut c_void);

    fn zuptsdk_easy_keygen(pub_path: *const c_char, priv_path: *const c_char) -> c_int;

    fn zuptsdk_easy_encrypt(
        pub_path: *const c_char,
        plaintext: *const u8, pt_sz: usize,
        blob_out: *mut *mut u8, blob_sz_out: *mut usize,
    ) -> c_int;

    fn zuptsdk_easy_decrypt(
        priv_path: *const c_char,
        blob: *const u8, blob_sz: usize,
        pt_out: *mut *mut u8, pt_sz_out: *mut usize,
    ) -> c_int;

    fn zuptsdk_easy_encrypt_password(
        password: *const c_char,
        plaintext: *const u8, pt_sz: usize,
        blob_out: *mut *mut u8, blob_sz_out: *mut usize,
    ) -> c_int;

    fn zuptsdk_easy_decrypt_password(
        password: *const c_char,
        blob: *const u8, blob_sz: usize,
        pt_out: *mut *mut u8, pt_sz_out: *mut usize,
    ) -> c_int;

    fn zuptsdk_easy_encrypt_file(
        pub_path: *const c_char,
        in_path: *const c_char,
        out_path: *const c_char,
        cb: *mut c_void, userdata: *mut c_void,
    ) -> c_int;

    fn zuptsdk_easy_decrypt_file(
        priv_path: *const c_char,
        in_path: *const c_char,
        out_path: *const c_char,
        cb: *mut c_void, userdata: *mut c_void,
    ) -> c_int;

    fn zuptsdk_easy_random_salt(out: *mut u8) -> c_int;
    fn zuptsdk_easy_derive_key(
        password: *const c_char, salt: *const u8, key_out: *mut u8,
    ) -> c_int;
}

// ─── Error type ───────────────────────────────────────────────────
#[derive(Debug)]
pub struct Error {
    pub code: i32,
    pub message: String,
    pub op: &'static str,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "libzuptsdk[{}]: code {}: {}", self.op, self.code, self.message)
    }
}

impl std::error::Error for Error {}

fn check(rc: c_int, op: &'static str) -> Result<(), Error> {
    if rc == 0 {
        Ok(())
    } else {
        let msg = unsafe {
            let p = zuptsdk_strerror(rc);
            if p.is_null() { String::from("unknown error") }
            else { CStr::from_ptr(p).to_string_lossy().into_owned() }
        };
        Err(Error { code: rc as i32, message: msg, op })
    }
}

fn cstr(s: &str) -> CString {
    CString::new(s).expect("string contains NUL")
}

fn cpath<P: AsRef<Path>>(p: P) -> CString {
    let s = p.as_ref().to_str().expect("path is not valid UTF-8");
    cstr(s)
}

// ─── Public API ───────────────────────────────────────────────────

/// Returns the runtime libzuptsdk version (e.g. "2.1.5").
pub fn version() -> &'static str {
    unsafe {
        CStr::from_ptr(zuptsdk_version_string())
            .to_str()
            .unwrap_or("?")
    }
}

/// Generates a hybrid (ML-KEM-768 + X25519) keypair.
pub fn keygen<P: AsRef<Path>, Q: AsRef<Path>>(pub_path: P, priv_path: Q) -> Result<(), Error> {
    let cp = cpath(pub_path);
    let cv = cpath(priv_path);
    check(unsafe { zuptsdk_easy_keygen(cp.as_ptr(), cv.as_ptr()) }, "keygen")
}

/// Encrypts plaintext for the recipient identified by their public key file.
pub fn encrypt<P: AsRef<Path>>(pub_path: P, plaintext: &[u8]) -> Result<Vec<u8>, Error> {
    let cp = cpath(pub_path);
    let mut blob: *mut u8 = ptr::null_mut();
    let mut blob_sz: usize = 0;
    let rc = unsafe {
        zuptsdk_easy_encrypt(
            cp.as_ptr(),
            plaintext.as_ptr(), plaintext.len(),
            &mut blob, &mut blob_sz,
        )
    };
    check(rc, "encrypt")?;
    let result = unsafe { slice::from_raw_parts(blob, blob_sz).to_vec() };
    unsafe { zuptsdk_free(blob as *mut c_void) };
    Ok(result)
}

/// Decrypts a blob with the recipient's private key.
pub fn decrypt<P: AsRef<Path>>(priv_path: P, blob: &[u8]) -> Result<Vec<u8>, Error> {
    let cp = cpath(priv_path);
    let mut pt: *mut u8 = ptr::null_mut();
    let mut pt_sz: usize = 0;
    let rc = unsafe {
        zuptsdk_easy_decrypt(
            cp.as_ptr(),
            blob.as_ptr(), blob.len(),
            &mut pt, &mut pt_sz,
        )
    };
    check(rc, "decrypt")?;
    let result = unsafe { slice::from_raw_parts(pt, pt_sz).to_vec() };
    unsafe { zuptsdk_free(pt as *mut c_void) };
    Ok(result)
}

/// Encrypts plaintext using a password (Argon2id KDF).
pub fn encrypt_password(password: &str, plaintext: &[u8]) -> Result<Vec<u8>, Error> {
    let cpw = cstr(password);
    let mut blob: *mut u8 = ptr::null_mut();
    let mut blob_sz: usize = 0;
    let rc = unsafe {
        zuptsdk_easy_encrypt_password(
            cpw.as_ptr(),
            plaintext.as_ptr(), plaintext.len(),
            &mut blob, &mut blob_sz,
        )
    };
    check(rc, "encrypt_password")?;
    let result = unsafe { slice::from_raw_parts(blob, blob_sz).to_vec() };
    unsafe { zuptsdk_free(blob as *mut c_void) };
    Ok(result)
}

/// Decrypts a password-encrypted blob.
pub fn decrypt_password(password: &str, blob: &[u8]) -> Result<Vec<u8>, Error> {
    let cpw = cstr(password);
    let mut pt: *mut u8 = ptr::null_mut();
    let mut pt_sz: usize = 0;
    let rc = unsafe {
        zuptsdk_easy_decrypt_password(
            cpw.as_ptr(),
            blob.as_ptr(), blob.len(),
            &mut pt, &mut pt_sz,
        )
    };
    check(rc, "decrypt_password")?;
    let result = unsafe { slice::from_raw_parts(pt, pt_sz).to_vec() };
    unsafe { zuptsdk_free(pt as *mut c_void) };
    Ok(result)
}

/// Encrypts a file.
pub fn encrypt_file<P, Q, R>(pub_path: P, in_path: Q, out_path: R) -> Result<(), Error>
where P: AsRef<Path>, Q: AsRef<Path>, R: AsRef<Path>
{
    let cp = cpath(pub_path);
    let ci = cpath(in_path);
    let co = cpath(out_path);
    check(unsafe {
        zuptsdk_easy_encrypt_file(cp.as_ptr(), ci.as_ptr(), co.as_ptr(),
                                   ptr::null_mut(), ptr::null_mut())
    }, "encrypt_file")
}

/// Decrypts a file.
pub fn decrypt_file<P, Q, R>(priv_path: P, in_path: Q, out_path: R) -> Result<(), Error>
where P: AsRef<Path>, Q: AsRef<Path>, R: AsRef<Path>
{
    let cp = cpath(priv_path);
    let ci = cpath(in_path);
    let co = cpath(out_path);
    check(unsafe {
        zuptsdk_easy_decrypt_file(cp.as_ptr(), ci.as_ptr(), co.as_ptr(),
                                   ptr::null_mut(), ptr::null_mut())
    }, "decrypt_file")
}

/// Generates a 16-byte random salt.
pub fn random_salt() -> Result<[u8; 16], Error> {
    let mut out = [0u8; 16];
    let rc = unsafe { zuptsdk_easy_random_salt(out.as_mut_ptr()) };
    check(rc, "random_salt")?;
    Ok(out)
}

/// Derives a deterministic 32-byte key from a password via Argon2id.
pub fn derive_key(password: &str, salt: &[u8; 16]) -> Result<[u8; 32], Error> {
    let cpw = cstr(password);
    let mut out = [0u8; 32];
    let rc = unsafe {
        zuptsdk_easy_derive_key(cpw.as_ptr(), salt.as_ptr(), out.as_mut_ptr())
    };
    check(rc, "derive_key")?;
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::TempDir;

    #[test]
    fn version_returns_string() {
        let v = version();
        assert!(v.len() >= 3);
    }

    #[test]
    fn roundtrip_pq() {
        let d = TempDir::new().unwrap();
        let pub_p = d.path().join("k.pub");
        let priv_p = d.path().join("k.priv");
        keygen(&pub_p, &priv_p).unwrap();

        let msg = b"Hello, post-quantum world!";
        let blob = encrypt(&pub_p, msg).unwrap();
        let plain = decrypt(&priv_p, &blob).unwrap();
        assert_eq!(plain, msg);
    }

    #[test]
    fn tamper_rejected() {
        let d = TempDir::new().unwrap();
        let pub_p = d.path().join("k.pub");
        let priv_p = d.path().join("k.priv");
        keygen(&pub_p, &priv_p).unwrap();

        let mut blob = encrypt(&pub_p, b"secret").unwrap();
        let mid = blob.len() / 2;
        blob[mid] ^= 0x01;
        assert!(decrypt(&priv_p, &blob).is_err());
    }

    #[test]
    fn password_roundtrip() {
        let blob = encrypt_password("correct horse battery staple", b"top secret").unwrap();
        let plain = decrypt_password("correct horse battery staple", &blob).unwrap();
        assert_eq!(plain, b"top secret");
        assert!(decrypt_password("wrong", &blob).is_err());
    }
}
