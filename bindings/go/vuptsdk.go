// Package vuptsdk provides Go bindings for libvuptsdk using cgo.
//
// Copyright (c) 2026 Cristian Cezar Moisés
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Quick start:
//
//	package main
//
//	import (
//	    "fmt"
//	    "github.com/cristiancmoises/libvuptsdk/bindings/go"
//	)
//
//	func main() {
//	    if err := vuptsdk.Keygen("alice.pub", "alice.priv"); err != nil {
//	        panic(err)
//	    }
//	    blob, err := vuptsdk.Encrypt("alice.pub", []byte("Hello"))
//	    if err != nil {
//	        panic(err)
//	    }
//	    plain, err := vuptsdk.Decrypt("alice.priv", blob)
//	    if err != nil {
//	        panic(err)
//	    }
//	    fmt.Printf("%s\n", plain) // "Hello"
//	}
package vuptsdk

/*
#cgo pkg-config: vuptsdk
#cgo LDFLAGS: -lvuptsdk
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdlib.h>
*/
import "C"
import (
	"errors"
	"fmt"
	"unsafe"
)

// Error wraps a libvuptsdk error code.
type Error struct {
	Code    int
	Message string
}

func (e *Error) Error() string {
	return fmt.Sprintf("libvuptsdk: code %d: %s", e.Code, e.Message)
}

func check(rc C.int, op string) error {
	if rc == 0 {
		return nil
	}
	msg := C.GoString(C.zuptsdk_strerror(rc))
	return &Error{Code: int(rc), Message: fmt.Sprintf("%s: %s", op, msg)}
}

// Version returns the libvuptsdk runtime version (e.g. "2.1.5").
func Version() string {
	return C.GoString(C.zuptsdk_version_string())
}

// Keygen generates a hybrid (ML-KEM-768 + X25519) keypair.
func Keygen(pubPath, privPath string) error {
	cPub := C.CString(pubPath)
	defer C.free(unsafe.Pointer(cPub))
	cPriv := C.CString(privPath)
	defer C.free(unsafe.Pointer(cPriv))
	return check(C.zuptsdk_easy_keygen(cPub, cPriv), "Keygen")
}

// Encrypt encrypts plaintext for the recipient's public key.
func Encrypt(pubPath string, plaintext []byte) ([]byte, error) {
	cPub := C.CString(pubPath)
	defer C.free(unsafe.Pointer(cPub))
	var blob *C.uint8_t
	var blobSz C.size_t
	rc := C.zuptsdk_easy_encrypt(
		cPub,
		(*C.uint8_t)(unsafe.Pointer(&plaintext[0])),
		C.size_t(len(plaintext)),
		&blob, &blobSz,
	)
	if err := check(rc, "Encrypt"); err != nil {
		return nil, err
	}
	defer C.zuptsdk_free(unsafe.Pointer(blob))
	out := C.GoBytes(unsafe.Pointer(blob), C.int(blobSz))
	return out, nil
}

// Decrypt recovers plaintext using the recipient's private key.
func Decrypt(privPath string, blob []byte) ([]byte, error) {
	cPriv := C.CString(privPath)
	defer C.free(unsafe.Pointer(cPriv))
	var pt *C.uint8_t
	var ptSz C.size_t
	rc := C.zuptsdk_easy_decrypt(
		cPriv,
		(*C.uint8_t)(unsafe.Pointer(&blob[0])),
		C.size_t(len(blob)),
		&pt, &ptSz,
	)
	if err := check(rc, "Decrypt"); err != nil {
		return nil, err
	}
	defer C.zuptsdk_free(unsafe.Pointer(pt))
	out := C.GoBytes(unsafe.Pointer(pt), C.int(ptSz))
	return out, nil
}

// EncryptPassword encrypts plaintext using Argon2id-derived key.
func EncryptPassword(password string, plaintext []byte) ([]byte, error) {
	cPw := C.CString(password)
	defer C.free(unsafe.Pointer(cPw))
	var blob *C.uint8_t
	var blobSz C.size_t
	rc := C.zuptsdk_easy_encrypt_password(
		cPw,
		(*C.uint8_t)(unsafe.Pointer(&plaintext[0])),
		C.size_t(len(plaintext)),
		&blob, &blobSz,
	)
	if err := check(rc, "EncryptPassword"); err != nil {
		return nil, err
	}
	defer C.zuptsdk_free(unsafe.Pointer(blob))
	return C.GoBytes(unsafe.Pointer(blob), C.int(blobSz)), nil
}

// DecryptPassword decrypts a password-encrypted blob.
func DecryptPassword(password string, blob []byte) ([]byte, error) {
	cPw := C.CString(password)
	defer C.free(unsafe.Pointer(cPw))
	var pt *C.uint8_t
	var ptSz C.size_t
	rc := C.zuptsdk_easy_decrypt_password(
		cPw,
		(*C.uint8_t)(unsafe.Pointer(&blob[0])),
		C.size_t(len(blob)),
		&pt, &ptSz,
	)
	if err := check(rc, "DecryptPassword"); err != nil {
		return nil, err
	}
	defer C.zuptsdk_free(unsafe.Pointer(pt))
	return C.GoBytes(unsafe.Pointer(pt), C.int(ptSz)), nil
}

// EncryptFile encrypts a file. No size limit.
func EncryptFile(pubPath, inPath, outPath string) error {
	cPub := C.CString(pubPath)
	cIn := C.CString(inPath)
	cOut := C.CString(outPath)
	defer C.free(unsafe.Pointer(cPub))
	defer C.free(unsafe.Pointer(cIn))
	defer C.free(unsafe.Pointer(cOut))
	return check(C.zuptsdk_easy_encrypt_file(cPub, cIn, cOut, nil, nil), "EncryptFile")
}

// DecryptFile decrypts a file.
func DecryptFile(privPath, inPath, outPath string) error {
	cPriv := C.CString(privPath)
	cIn := C.CString(inPath)
	cOut := C.CString(outPath)
	defer C.free(unsafe.Pointer(cPriv))
	defer C.free(unsafe.Pointer(cIn))
	defer C.free(unsafe.Pointer(cOut))
	return check(C.zuptsdk_easy_decrypt_file(cPriv, cIn, cOut, nil, nil), "DecryptFile")
}

// RandomSalt generates a 16-byte random salt.
func RandomSalt() ([]byte, error) {
	out := make([]byte, 16)
	rc := C.zuptsdk_easy_random_salt((*C.uint8_t)(unsafe.Pointer(&out[0])))
	if err := check(rc, "RandomSalt"); err != nil {
		return nil, err
	}
	return out, nil
}

// DeriveKey derives a 32-byte key from password via Argon2id.
func DeriveKey(password string, salt []byte) ([]byte, error) {
	if len(salt) != 16 {
		return nil, errors.New("salt must be 16 bytes")
	}
	cPw := C.CString(password)
	defer C.free(unsafe.Pointer(cPw))
	out := make([]byte, 32)
	rc := C.zuptsdk_easy_derive_key(
		cPw,
		(*C.uint8_t)(unsafe.Pointer(&salt[0])),
		(*C.uint8_t)(unsafe.Pointer(&out[0])),
	)
	if err := check(rc, "DeriveKey"); err != nil {
		return nil, err
	}
	return out, nil
}
