/*
 * libvuptsdk smoke test
 * Copyright (c) 2026 Cristian Cezar Moisés
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Exercises the documented public API (only functions declared in
 * <zuptsdk.h> and <zuptsdk_easy.h>). Returns 0 on success, 1 on any failure.
 */
#define _DEFAULT_SOURCE 1
#define _XOPEN_SOURCE   700
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static int g_pass = 0, g_fail = 0;

#define TEST(name) fprintf(stderr, "  %-55s", name); fflush(stderr)
#define PASS()     do { fprintf(stderr, "PASS\n"); g_pass++; } while (0)
#define FAIL(msg)  do { fprintf(stderr, "FAIL (%s)\n", msg); g_fail++; } while (0)

static void test_version(void) {
    TEST("version string is well-formed");
    const char *v = zuptsdk_version_string();
    if (!v || strlen(v) < 3) { FAIL("empty"); return; }
    if (v[0] < '0' || v[0] > '9') { FAIL("doesn't start with digit"); return; }
    PASS();
}

static void test_version_check(void) {
    TEST("version_check rejects nonsense");
    int rc = zuptsdk_version_check(99, 99, 99);
    if (rc == 0) { FAIL("accepted impossible version"); return; }
    PASS();
}

static void test_strerror(void) {
    TEST("strerror handles arbitrary code");
    const char *s1 = zuptsdk_strerror(0);
    const char *s2 = zuptsdk_strerror(-99999);
    if (!s1 || !s2) { FAIL("returned NULL"); return; }
    PASS();
}

static void test_secure_zero(void) {
    TEST("secure_zero clears memory");
    uint8_t buf[64];
    memset(buf, 0xAA, sizeof(buf));
    zuptsdk_secure_zero(buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); i++) {
        if (buf[i] != 0) { FAIL("non-zero byte"); return; }
    }
    PASS();
}

static void test_random_salt(void) {
    TEST("easy_random_salt yields entropy");
    uint8_t salt1[16], salt2[16];
    int rc1 = zuptsdk_easy_random_salt(salt1);
    int rc2 = zuptsdk_easy_random_salt(salt2);
    if (rc1 != 0 || rc2 != 0) { FAIL("non-zero return"); return; }
    if (memcmp(salt1, salt2, 16) == 0) { FAIL("same bytes twice"); return; }
    PASS();
}

static void test_easy_keygen(void) {
    TEST("easy_keygen creates priv/pub files");
    char privf[] = "/tmp/lzsdk_kgA_XXXXXX"; int pfd = mkstemp(privf); if (pfd >= 0) close(pfd);
    char pubf[]  = "/tmp/lzsdk_kgB_XXXXXX"; int ufd = mkstemp(pubf);  if (ufd >= 0) close(ufd);

    int rc = zuptsdk_easy_keygen(pubf, privf);
    if (rc != 0) { FAIL("rc != 0"); unlink(privf); unlink(pubf); return; }

    struct stat st;
    if (stat(privf, &st) != 0 || st.st_size == 0) { FAIL("priv file empty"); unlink(privf); unlink(pubf); return; }
    if (stat(pubf, &st)  != 0 || st.st_size == 0) { FAIL("pub file empty");  unlink(privf); unlink(pubf); return; }

    unlink(privf); unlink(pubf);
    PASS();
}

static void test_easy_pq_roundtrip(void) {
    TEST("easy_encrypt / easy_decrypt round-trip");

    char privf[] = "/tmp/lzsdk_rtA_XXXXXX"; int pfd = mkstemp(privf); if (pfd >= 0) close(pfd);
    char pubf[]  = "/tmp/lzsdk_rtB_XXXXXX"; int ufd = mkstemp(pubf);  if (ufd >= 0) close(ufd);

    if (zuptsdk_easy_keygen(pubf, privf) != 0) { FAIL("keygen"); unlink(privf); unlink(pubf); return; }

    const char *plaintext = "hello libvuptsdk PQ hybrid encryption!";
    uint8_t *blob = NULL; size_t blob_sz = 0;
    int rc = zuptsdk_easy_encrypt(pubf, (const uint8_t *)plaintext,
                                   strlen(plaintext), &blob, &blob_sz);
    if (rc != 0 || !blob) { FAIL("easy_encrypt"); unlink(privf); unlink(pubf); return; }

    uint8_t *out = NULL; size_t out_sz = 0;
    rc = zuptsdk_easy_decrypt(privf, blob, blob_sz, &out, &out_sz);
    if (rc != 0 || !out) { FAIL("easy_decrypt"); zuptsdk_free(blob); unlink(privf); unlink(pubf); return; }

    if (out_sz != strlen(plaintext) || memcmp(out, plaintext, out_sz) != 0) {
        FAIL("plaintext mismatch"); zuptsdk_free(blob); zuptsdk_free(out);
        unlink(privf); unlink(pubf); return;
    }

    zuptsdk_free(blob); zuptsdk_free(out);
    unlink(privf); unlink(pubf);
    PASS();
}

static void test_wrong_key_rejection(void) {
    TEST("wrong-key rejection");

    char privA[] = "/tmp/lzsdk_pA_XXXXXX"; int a1 = mkstemp(privA); if (a1>=0) close(a1);
    char pubA[]  = "/tmp/lzsdk_uA_XXXXXX"; int a2 = mkstemp(pubA);  if (a2>=0) close(a2);
    char privB[] = "/tmp/lzsdk_pB_XXXXXX"; int b1 = mkstemp(privB); if (b1>=0) close(b1);
    char pubB[]  = "/tmp/lzsdk_uB_XXXXXX"; int b2 = mkstemp(pubB);  if (b2>=0) close(b2);

    zuptsdk_easy_keygen(pubA, privA);
    zuptsdk_easy_keygen(pubB, privB);

    const char *pt = "secret message";
    uint8_t *blob = NULL; size_t blob_sz = 0;
    int rc = zuptsdk_easy_encrypt(pubA, (const uint8_t *)pt, strlen(pt), &blob, &blob_sz);
    if (rc != 0) {
        FAIL("encrypt setup");
        unlink(privA); unlink(pubA); unlink(privB); unlink(pubB);
        return;
    }

    uint8_t *out = NULL; size_t out_sz = 0;
    rc = zuptsdk_easy_decrypt(privB, blob, blob_sz, &out, &out_sz);
    if (rc == 0) {
        FAIL("wrong key accepted");
        zuptsdk_free(out); zuptsdk_free(blob);
        unlink(privA); unlink(pubA); unlink(privB); unlink(pubB);
        return;
    }

    zuptsdk_free(blob);
    unlink(privA); unlink(pubA); unlink(privB); unlink(pubB);
    PASS();
}

static void test_tamper_detection(void) {
    TEST("tamper detection (single bit flip)");

    char privf[] = "/tmp/lzsdk_tA_XXXXXX"; int pfd = mkstemp(privf); if (pfd>=0) close(pfd);
    char pubf[]  = "/tmp/lzsdk_tB_XXXXXX"; int ufd = mkstemp(pubf);  if (ufd>=0) close(ufd);
    zuptsdk_easy_keygen(pubf, privf);

    uint8_t *blob = NULL; size_t blob_sz = 0;
    int rc = zuptsdk_easy_encrypt(pubf, (const uint8_t *)"AAAA", 4, &blob, &blob_sz);
    if (rc != 0 || !blob || blob_sz < 64) { FAIL("encrypt"); unlink(privf); unlink(pubf); return; }

    blob[blob_sz / 2] ^= 0x01;

    uint8_t *out = NULL; size_t out_sz = 0;
    rc = zuptsdk_easy_decrypt(privf, blob, blob_sz, &out, &out_sz);
    if (rc == 0) {
        FAIL("tampered blob accepted");
        zuptsdk_free(out); zuptsdk_free(blob);
        unlink(privf); unlink(pubf); return;
    }

    zuptsdk_free(blob);
    unlink(privf); unlink(pubf);
    PASS();
}

static void test_easy_password_roundtrip(void) {
    TEST("easy_encrypt_password / easy_decrypt_password");
    const char *pt = "data encrypted with a password";
    const char *pw = "correct horse battery staple";
    uint8_t *blob = NULL; size_t blob_sz = 0;
    int rc = zuptsdk_easy_encrypt_password(pw, (const uint8_t *)pt, strlen(pt),
                                            &blob, &blob_sz);
    if (rc != 0 || !blob) { FAIL("encrypt_password"); return; }

    uint8_t *out = NULL; size_t out_sz = 0;
    rc = zuptsdk_easy_decrypt_password(pw, blob, blob_sz, &out, &out_sz);
    if (rc != 0 || !out) { FAIL("decrypt_password"); zuptsdk_free(blob); return; }

    if (out_sz != strlen(pt) || memcmp(out, pt, out_sz) != 0) {
        FAIL("plaintext mismatch"); zuptsdk_free(blob); zuptsdk_free(out); return;
    }

    zuptsdk_free(out); out = NULL; out_sz = 0;
    rc = zuptsdk_easy_decrypt_password("WRONG", blob, blob_sz, &out, &out_sz);
    if (rc == 0) { FAIL("wrong password accepted"); zuptsdk_free(blob); zuptsdk_free(out); return; }

    zuptsdk_free(blob);
    PASS();
}

int main(void) {
    fprintf(stderr, "═══════════════════════════════════════════════════════════\n");
    fprintf(stderr, "  libvuptsdk %s smoke test\n", zuptsdk_version_string());
    fprintf(stderr, "═══════════════════════════════════════════════════════════\n");

    test_version();
    test_version_check();
    test_strerror();
    test_secure_zero();
    test_random_salt();
    test_easy_keygen();
    test_easy_pq_roundtrip();
    test_wrong_key_rejection();
    test_tamper_detection();
    test_easy_password_roundtrip();

    fprintf(stderr, "\n  ─────────────────────────────────\n");
    fprintf(stderr, "  Smoke test: %d passed, %d failed\n", g_pass, g_fail);
    fprintf(stderr, "  ─────────────────────────────────\n\n");
    return g_fail == 0 ? 0 : 1;
}
