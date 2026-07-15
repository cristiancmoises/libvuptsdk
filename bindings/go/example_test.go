// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Cristian Cezar Moisés
package vuptsdk_test

import (
	"bytes"
	"fmt"
	"os"
	"path/filepath"
	"testing"

	zupt "git.securityops.co/cristiancmoises/libvuptsdk/bindings/go"
)

func ExampleVersion() {
	fmt.Printf("libvuptsdk %s\n", zupt.Version())
}

func TestRoundtrip(t *testing.T) {
	d := t.TempDir()
	pub := filepath.Join(d, "k.pub")
	priv := filepath.Join(d, "k.priv")

	if err := zupt.Keygen(pub, priv); err != nil {
		t.Fatal(err)
	}

	msg := []byte("Hello, post-quantum world!")
	blob, err := zupt.Encrypt(pub, msg)
	if err != nil {
		t.Fatal(err)
	}

	plain, err := zupt.Decrypt(priv, blob)
	if err != nil {
		t.Fatal(err)
	}

	if !bytes.Equal(msg, plain) {
		t.Errorf("got %q, want %q", plain, msg)
	}
}

func TestTamperRejected(t *testing.T) {
	d := t.TempDir()
	pub := filepath.Join(d, "k.pub")
	priv := filepath.Join(d, "k.priv")
	if err := zupt.Keygen(pub, priv); err != nil {
		t.Fatal(err)
	}

	blob, err := zupt.Encrypt(pub, []byte("secret"))
	if err != nil {
		t.Fatal(err)
	}

	// Flip a bit
	blob[len(blob)/2] ^= 0x01

	if _, err := zupt.Decrypt(priv, blob); err == nil {
		t.Fatal("expected decryption to fail on tampered blob")
	}
}

func TestPasswordMode(t *testing.T) {
	pw := "correct horse battery staple"
	msg := []byte("top secret")
	blob, err := zupt.EncryptPassword(pw, msg)
	if err != nil {
		t.Fatal(err)
	}
	plain, err := zupt.DecryptPassword(pw, blob)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(msg, plain) {
		t.Errorf("got %q", plain)
	}

	if _, err := zupt.DecryptPassword("wrong password", blob); err == nil {
		t.Fatal("expected wrong password to fail")
	}
}

func TestFileMode(t *testing.T) {
	d := t.TempDir()
	pub := filepath.Join(d, "k.pub")
	priv := filepath.Join(d, "k.priv")
	zupt.Keygen(pub, priv)

	plainPath := filepath.Join(d, "doc.txt")
	encPath := filepath.Join(d, "doc.enc")
	recPath := filepath.Join(d, "doc-out.txt")

	original := []byte("file content for the test")
	os.WriteFile(plainPath, original, 0644)

	if err := zupt.EncryptFile(pub, plainPath, encPath); err != nil {
		t.Fatal(err)
	}
	if err := zupt.DecryptFile(priv, encPath, recPath); err != nil {
		t.Fatal(err)
	}
	got, _ := os.ReadFile(recPath)
	if !bytes.Equal(got, original) {
		t.Error("file content mismatch")
	}
}
