/*
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-libvuptsdk-Commercial
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
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

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

    TEST("failed calls reset transfer outputs");
    {
        uint8_t *archive = (uint8_t *)(uintptr_t)1;
        uint8_t *output = (uint8_t *)(uintptr_t)1;
        size_t archive_size = 1, output_size = 1;
        zuptsdk_archive_info_t *info =
            (zuptsdk_archive_info_t *)(uintptr_t)1;
        zuptsdk_privkey_t *key = (zuptsdk_privkey_t *)(uintptr_t)1;
        zuptsdk_ctx_t *ctx = (zuptsdk_ctx_t *)(uintptr_t)1;
        zuptsdk_options_t *opts = (zuptsdk_options_t *)(uintptr_t)1;
        zuptsdk_secure_buf_t *secure =
            (zuptsdk_secure_buf_t *)(uintptr_t)1;
        uint8_t *borrowed = (uint8_t *)(uintptr_t)1;
        size_t borrowed_size = 1;
        int a = zuptsdk_compress_buffer(NULL, NULL, "x", NULL, 0, NULL,
                                        NULL, &archive, &archive_size);
        int b = zuptsdk_extract_buffer(NULL, NULL, 0, NULL, NULL, &output,
                                       &output_size);
        int c = zuptsdk_archive_info_read(NULL, NULL, 0, &info);
        int d = zuptsdk_privkey_load(NULL, &key);
        int e = zuptsdk_secure_buf_from_data(NULL, 1, &secure);
        int f = zuptsdk_secure_buf_get(NULL, &borrowed, &borrowed_size);
        if (zuptsdk_ctx_create(&ctx) == ZUPTSDK_OK)
            zuptsdk_ctx_destroy(ctx);
        if (zuptsdk_options_create(&opts) == ZUPTSDK_OK)
            zuptsdk_options_destroy(opts);

        if (a == ZUPTSDK_ERR_INVALID_ARG && b == ZUPTSDK_ERR_INVALID_ARG &&
            c == ZUPTSDK_ERR_INVALID_ARG && d == ZUPTSDK_ERR_INVALID_ARG &&
            e == ZUPTSDK_ERR_INVALID_ARG && f == ZUPTSDK_ERR_INVALID_ARG &&
            archive == NULL && archive_size == 0 && output == NULL &&
            output_size == 0 && info == NULL && key == NULL &&
            secure == NULL && borrowed == NULL && borrowed_size == 0 &&
            ctx != (zuptsdk_ctx_t *)(uintptr_t)1 &&
            opts != (zuptsdk_options_t *)(uintptr_t)1) {
            PASS();
        } else {
            FAIL("output pointers retained stale values");
        }
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

    TEST("context extraction limit validates arguments");
    {
        zuptsdk_ctx_t *c = NULL;
        int rc = zuptsdk_ctx_set_max_decompressed(NULL, 1024);
        if (rc != ZUPTSDK_ERR_INVALID_ARG ||
            zuptsdk_ctx_create(&c) != ZUPTSDK_OK ||
            zuptsdk_ctx_set_max_decompressed(c, 1024) != ZUPTSDK_OK) {
            FAIL("limit setter contract");
        } else {
            PASS();
        }
        zuptsdk_ctx_destroy(c);
    }

#ifndef _WIN32
    TEST("hybrid-key archive and private output policy");
    {
        char dir[] = "/tmp/libvuptsdk-key-test.XXXXXX";
        char private_path[256] = {0};
        char public_path[256] = {0};
        char victim_path[256] = {0};
        char link_path[256] = {0};
        char fifo_path[256] = {0};
        zuptsdk_ctx_t *ctx = NULL;
        zuptsdk_keypair_t *kp = NULL;
        zuptsdk_pubkey_t *public_key = NULL;
        zuptsdk_privkey_t *private_key = NULL;
        zuptsdk_options_t *opts = NULL;
        uint8_t *archive = NULL, *output = NULL;
        size_t archive_size = 0, output_size = 0;
        static const uint8_t payload[] = "hybrid archive test";
        FILE *f = NULL;
        struct stat st;
        char victim[8] = {0};
        int rc = ZUPTSDK_ERR_INTERNAL;

        if (mkdtemp(dir) &&
            snprintf(private_path, sizeof(private_path), "%s/private.key", dir) > 0 &&
            snprintf(public_path, sizeof(public_path), "%s/public.key", dir) > 0 &&
            snprintf(victim_path, sizeof(victim_path), "%s/victim", dir) > 0 &&
            snprintf(link_path, sizeof(link_path), "%s/link.key", dir) > 0 &&
            snprintf(fifo_path, sizeof(fifo_path), "%s/hybrid.bin", dir) > 0 &&
            zuptsdk_ctx_create(&ctx) == ZUPTSDK_OK &&
            zuptsdk_keypair_generate(ctx, &kp) == ZUPTSDK_OK) {
            mode_t old_mask = umask(0);
            f = fopen(private_path, "wb");
            if (f) fclose(f);
            f = fopen(victim_path, "wb");
            if (f) {
                (void)fwrite("intact", 1, 6, f);
                fclose(f);
            }
            if (symlink(victim_path, link_path) == 0 &&
                mkfifo(fifo_path, 0600) == 0 &&
                zuptsdk_keypair_save_private(kp, private_path) == ZUPTSDK_OK &&
                stat(private_path, &st) == 0 &&
                (st.st_mode & 0777) == 0600 &&
                zuptsdk_keypair_save_private(kp, link_path) == ZUPTSDK_ERR_IO &&
                zuptsdk_keypair_save_private(kp, fifo_path) == ZUPTSDK_ERR_IO) {
                f = fopen(victim_path, "rb");
                if (f) {
                    size_t n = fread(victim, 1, 6, f);
                    fclose(f);
                    if (n == 6 && memcmp(victim, "intact", 6) == 0)
                        rc = ZUPTSDK_OK;
                }
            }
            umask(old_mask);

            if (rc == ZUPTSDK_OK &&
                zuptsdk_keypair_save_public(kp, public_path) == ZUPTSDK_OK &&
                zuptsdk_pubkey_load(public_path, &public_key) == ZUPTSDK_OK &&
                zuptsdk_privkey_load(private_path, &private_key) == ZUPTSDK_OK &&
                zuptsdk_options_create(&opts) == ZUPTSDK_OK &&
                zuptsdk_options_set_codec(opts, ZUPTSDK_CODEC_VAPTVUPT) == ZUPTSDK_OK &&
                zuptsdk_compress_buffer(ctx, opts, "hybrid.bin", payload,
                                        sizeof(payload) - 1, NULL, public_key,
                                        &archive, &archive_size) == ZUPTSDK_OK &&
                zuptsdk_extract_to_dir(ctx, archive, archive_size, dir,
                                       NULL, private_key) != ZUPTSDK_OK &&
                unlink(fifo_path) == 0 &&
                zuptsdk_extract_buffer(ctx, archive, archive_size, NULL,
                                       private_key, &output,
                                       &output_size) == ZUPTSDK_OK &&
                output_size == sizeof(payload) - 1 &&
                memcmp(output, payload, output_size) == 0) {
                rc = ZUPTSDK_OK;
            } else {
                rc = ZUPTSDK_ERR_INTERNAL;
            }
        }

        if (rc == ZUPTSDK_OK) PASS();
        else FAIL("hybrid archive or private-key output policy");

        zuptsdk_free(output);
        zuptsdk_free(archive);
        zuptsdk_options_destroy(opts);
        zuptsdk_privkey_destroy(private_key);
        zuptsdk_pubkey_destroy(public_key);
        zuptsdk_keypair_destroy(kp);
        zuptsdk_ctx_destroy(ctx);
        if (link_path[0]) unlink(link_path);
        if (fifo_path[0]) unlink(fifo_path);
        if (private_path[0]) unlink(private_path);
        if (public_path[0]) unlink(public_path);
        if (victim_path[0]) unlink(victim_path);
        if (dir[0]) rmdir(dir);
    }
#endif

    TEST("VaptVupt archive buffer round-trip");
    {
        static const uint8_t pattern[] = "libvuptsdk codec 2.65.11 ";
        uint8_t input[32768];
        uint8_t *archive = NULL;
        uint8_t *output = NULL;
        size_t archive_size = 0;
        size_t output_size = 0;
        zuptsdk_ctx_t *ctx = NULL;
        zuptsdk_options_t *opts = NULL;
        int rc = ZUPTSDK_OK;

        for (size_t i = 0; i < sizeof(input); i++)
            input[i] = pattern[i % (sizeof(pattern) - 1)];

        if (zuptsdk_ctx_create(&ctx) != ZUPTSDK_OK ||
            zuptsdk_options_create(&opts) != ZUPTSDK_OK ||
            zuptsdk_ctx_set_threads(ctx, 2) != ZUPTSDK_OK ||
            zuptsdk_options_set_codec(opts, ZUPTSDK_CODEC_VAPTVUPT) != ZUPTSDK_OK ||
            zuptsdk_options_set_level(opts, 5) != ZUPTSDK_OK) {
            rc = ZUPTSDK_ERR_INTERNAL;
        }
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_compress_buffer(ctx, opts, "payload.bin",
                                         input, sizeof(input), NULL, NULL,
                                         &archive, &archive_size);
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_verify(ctx, archive, archive_size, NULL, NULL);
        if (rc == ZUPTSDK_OK) {
            zuptsdk_archive_info_t *info = NULL;
            rc = zuptsdk_archive_info_read(ctx, archive, archive_size, &info);
            if (rc == ZUPTSDK_OK &&
                (zuptsdk_archive_info_format_major(info) <= 0 ||
                 strlen(zuptsdk_archive_info_uuid(info)) != 36 ||
                 zuptsdk_archive_info_created_unix(info) <= 0 ||
                 zuptsdk_archive_info_block_count(info) == 0)) {
                rc = ZUPTSDK_ERR_INTERNAL;
            }
            zuptsdk_archive_info_destroy(info);
        }
        if (rc == ZUPTSDK_OK && archive_size >= 4) {
            size_t version_offset = archive_size - 4;
            uint8_t saved_version = archive[version_offset];
            zuptsdk_archive_info_t *info = NULL;
            archive[version_offset] = 2;
            rc = zuptsdk_archive_info_read(ctx, archive, archive_size, &info);
            archive[version_offset] = saved_version;
            if (rc == ZUPTSDK_OK &&
                zuptsdk_archive_info_block_count(info) != 0) {
                rc = ZUPTSDK_ERR_INTERNAL;
            }
            zuptsdk_archive_info_destroy(info);
        }
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_ctx_set_max_decompressed(ctx, 1024);
        if (rc == ZUPTSDK_OK) {
            int limit_rc = zuptsdk_extract_buffer(ctx, archive, archive_size,
                                                  NULL, NULL, &output,
                                                  &output_size);
            if (limit_rc != ZUPTSDK_ERR_TOO_LARGE || output != NULL ||
                output_size != 0) {
                rc = ZUPTSDK_ERR_INTERNAL;
            }
        }
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_ctx_set_max_decompressed(ctx, 16ull * 1024 * 1024);
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_extract_buffer(ctx, archive, archive_size,
                                        NULL, NULL, &output, &output_size);

        if (rc == ZUPTSDK_OK && archive && archive_size < sizeof(input) &&
            output && output_size == sizeof(input) &&
            memcmp(input, output, sizeof(input)) == 0) {
            PASS();
        } else {
            FAIL(zuptsdk_last_error_detail());
        }

        zuptsdk_free(output);
        zuptsdk_free(archive);
        zuptsdk_options_destroy(opts);
        zuptsdk_ctx_destroy(ctx);
    }

    TEST("password archive authenticates and round-trips");
    {
        static const uint8_t input[] =
            "authenticated archive payload authenticated archive payload";
        static const uint8_t password[] = "correct test password";
        static const uint8_t wrong_password[] = "incorrect test password";
        zuptsdk_ctx_t *ctx = NULL;
        zuptsdk_options_t *opts = NULL;
        zuptsdk_secure_buf_t *good = NULL, *wrong = NULL;
        uint8_t *archive = NULL, *output = NULL;
        size_t archive_size = 0, output_size = 0;
        int rc = zuptsdk_ctx_create(&ctx);

        if (rc == ZUPTSDK_OK) rc = zuptsdk_options_create(&opts);
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_options_set_codec(opts, ZUPTSDK_CODEC_VAPTVUPT);
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_secure_buf_from_data(password, sizeof(password) - 1,
                                               &good);
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_secure_buf_from_data(wrong_password,
                                               sizeof(wrong_password) - 1,
                                               &wrong);
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_compress_buffer(ctx, opts, "secret.bin", input,
                                         sizeof(input) - 1, good, NULL,
                                         &archive, &archive_size);
        if (rc == ZUPTSDK_OK) {
            int wrong_rc = zuptsdk_extract_buffer(ctx, archive, archive_size,
                                                  wrong, NULL, &output,
                                                  &output_size);
            if (wrong_rc != ZUPTSDK_ERR_BAD_PASSWORD || output != NULL ||
                output_size != 0) {
                rc = ZUPTSDK_ERR_INTERNAL;
            }
        }
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_extract_buffer(ctx, archive, archive_size, good, NULL,
                                        &output, &output_size);

        if (rc == ZUPTSDK_OK && output && output_size == sizeof(input) - 1 &&
            memcmp(input, output, output_size) == 0) {
            PASS();
        } else {
            FAIL(zuptsdk_last_error_detail());
        }

        zuptsdk_free(output);
        zuptsdk_free(archive);
        zuptsdk_secure_buf_destroy(wrong);
        zuptsdk_secure_buf_destroy(good);
        zuptsdk_options_destroy(opts);
        zuptsdk_ctx_destroy(ctx);
    }

#ifndef _WIN32
    TEST("malformed solid archive leaves no output file");
    {
        static const uint8_t input[] =
            "solid archive payload solid archive payload";
        char dir[] = "/tmp/libvuptsdk-solid-test.XXXXXX";
        char output_path[512] = {0};
        zuptsdk_ctx_t *ctx = NULL;
        zuptsdk_options_t *opts = NULL;
        uint8_t *archive = NULL;
        size_t archive_size = 0;
        int rc = zuptsdk_ctx_create(&ctx);

        if (rc == ZUPTSDK_OK) rc = zuptsdk_options_create(&opts);
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_options_set_codec(opts, ZUPTSDK_CODEC_VAPTVUPT);
        if (rc == ZUPTSDK_OK) rc = zuptsdk_options_set_solid(opts, 1);
        if (rc == ZUPTSDK_OK)
            rc = zuptsdk_compress_buffer(ctx, opts, "solid.bin", input,
                                         sizeof(input) - 1, NULL, NULL,
                                         &archive, &archive_size);
        if (rc == ZUPTSDK_OK && archive_size > 66 && mkdtemp(dir)) {
            /* An unencrypted archive begins its first block after the 64-byte
             * header. Turn that data block into a premature index block. */
            archive[66] = 0x02;
            if (snprintf(output_path, sizeof(output_path), "%s/solid.bin", dir) <= 0 ||
                zuptsdk_extract_to_dir(ctx, archive, archive_size, dir,
                                       NULL, NULL) == ZUPTSDK_OK ||
                access(output_path, F_OK) == 0 || rmdir(dir) != 0) {
                rc = ZUPTSDK_ERR_INTERNAL;
            }
        } else {
            rc = ZUPTSDK_ERR_INTERNAL;
        }

        if (rc == ZUPTSDK_OK) PASS();
        else FAIL("malformed solid stream was accepted or left output");

        if (output_path[0]) unlink(output_path);
        if (dir[0]) rmdir(dir);
        zuptsdk_free(archive);
        zuptsdk_options_destroy(opts);
        zuptsdk_ctx_destroy(ctx);
    }
#endif

    fprintf(stderr, "\n  ─────────────────────────────────\n");
    fprintf(stderr, "  Source smoke: %d passed, %d failed\n", pass, fail);
    fprintf(stderr, "  ─────────────────────────────────\n");

    return fail > 0 ? 1 : 0;
}
