/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Cristian Cezar Moisés
 *
 * libvuptsdk source-only smoke test
 *
 * Exercises the public API symbols that exist in the from-source build
 * (libvuptsdk-base.so). Used by `make test-asan` to verify ASAN/UBSAN
 * cleanliness without depending on the canonical prebuilt binary.
 *
 * Returns 0 on full pass, 1 on any failure.
 */
#define _DEFAULT_SOURCE 1
#include <zuptsdk.h>
#include <stdio.h>
#include <string.h>

#define TEST(name) fprintf(stderr, "  %-55s", name); fflush(stderr)
#define PASS()     do { fprintf(stderr, "PASS\n"); pass++; } while (0)
#define FAIL(msg)  do { fprintf(stderr, "FAIL (%s)\n", msg); fail++; } while (0)

int main(void) {
    int pass = 0, fail = 0;

    fprintf(stderr, "═══════════════════════════════════════════════════════════\n");
    fprintf(stderr, "  libvuptsdk %s source-only smoke test\n",
            zuptsdk_version_string());
    fprintf(stderr, "═══════════════════════════════════════════════════════════\n");

    TEST("version string is well-formed");
    {
        const char *v = zuptsdk_version_string();
        if (!v || strlen(v) < 3) FAIL("empty");
        else if (v[0] < '0' || v[0] > '9') FAIL("doesn't start with digit");
        else PASS();
    }

    TEST("version_check rejects nonsense");
    if (zuptsdk_version_check(99, 99, 99) != 0) PASS();
    else FAIL("accepted bogus version");

    TEST("strerror handles arbitrary code");
    {
        const char *e = zuptsdk_strerror(-9999);
        if (e && strlen(e) > 0) PASS();
        else FAIL("returned null");
    }

    TEST("secure_zero clears memory");
    {
        char buf[64];
        memset(buf, 0xAA, sizeof(buf));
        zuptsdk_secure_zero(buf, sizeof(buf));
        int ok = 1;
        for (size_t i = 0; i < sizeof(buf); i++) if (buf[i]) { ok = 0; break; }
        if (ok) PASS();
        else FAIL("not all bytes zero");
    }

    TEST("options_create / options_destroy");
    {
        zuptsdk_options_t *o = NULL;
        int rc = zuptsdk_options_create(&o);
        if (rc == 0 && o) {
            zuptsdk_options_destroy(o);
            PASS();
        } else FAIL("options_create failed");
    }

    TEST("ctx_create / ctx_destroy");
    {
        zuptsdk_ctx_t *c = NULL;
        int rc = zuptsdk_ctx_create(&c);
        if (rc == 0 && c) {
            zuptsdk_ctx_destroy(c);
            PASS();
        } else FAIL("ctx_create failed");
    }

    fprintf(stderr, "\n  ─────────────────────────────────\n");
    fprintf(stderr, "  Source smoke: %d passed, %d failed\n", pass, fail);
    fprintf(stderr, "  ─────────────────────────────────\n");

    return fail > 0 ? 1 : 0;
}
