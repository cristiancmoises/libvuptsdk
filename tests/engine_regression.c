/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Cristian Cezar Moisés
 * Public SDK/embedded-engine integration regressions. */
#define _DEFAULT_SOURCE 1
#include "zuptsdk.h"
#include "zupt.h"
#include "zupt_cpuid.h"
#include "zupt_mlkem.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks, failures;
#define CHECK(name, expression) do { \
    checks++; \
    if (!(expression)) { failures++; fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__); } \
} while (0)

static int write_file(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}

static int file_equals(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t *buffer = malloc(size ? size : 1);
    int ok = buffer && fread(buffer, 1, size, f) == size &&
             memcmp(buffer, data, size) == 0 && fgetc(f) == EOF;
    free(buffer);
    fclose(f);
    return ok;
}

static size_t next_frame(const uint8_t *archive, size_t size, size_t offset) {
    if (offset > size || size - offset < 7 || archive[offset] != 0xbb || archive[offset + 1] != 1)
        return 0;
    size_t cursor = offset + 7;
    uint64_t raw_size, stored_size;
    int count = zupt_decode_varint(archive + cursor, size - cursor, &raw_size);
    if (count < 0) return 0;
    cursor += (size_t)count;
    count = zupt_decode_varint(archive + cursor, size - cursor, &stored_size);
    if (count < 0) return 0;
    cursor += (size_t)count;
    if (size - cursor < 8 || stored_size > size - cursor - 8) return 0;
    return cursor + 8 + (size_t)stored_size;
}

static void roundtrip(int level, int solid, int dedup, int encrypted, int threads) {
    enum { INPUT_SIZE = 196608 };
    uint8_t *input = malloc(INPUT_SIZE), *archive = NULL, *output = NULL;
    size_t archive_size = 0, output_size = 0;
    zuptsdk_ctx_t *ctx = NULL;
    zuptsdk_options_t *opts = NULL;
    zuptsdk_secure_buf_t *password = NULL;
    for (size_t i = 0; input && i < INPUT_SIZE; i++) input[i] = (uint8_t)((i % 65536) % 251);
    int rc = input ? zuptsdk_ctx_create(&ctx) : ZUPTSDK_ERR_NO_MEMORY;
    if (!rc) rc = zuptsdk_options_create(&opts);
    if (!rc) rc = zuptsdk_options_set_level(opts, level);
    if (!rc) rc = zuptsdk_options_set_codec(opts, ZUPTSDK_CODEC_VAPTVUPT);
    if (!rc) rc = zuptsdk_options_set_solid(opts, solid);
    if (!rc) rc = zuptsdk_options_set_dedup(opts, dedup);
    if (!rc) rc = zuptsdk_options_set_block_size(opts, 65536);
    if (!rc) rc = zuptsdk_ctx_set_threads(ctx, threads);
    if (!rc && encrypted) rc = zuptsdk_secure_buf_from_data((const uint8_t *)"engine-regression", 17, &password);
    if (!rc) rc = zuptsdk_compress_buffer(ctx, opts, "payload.bin", input, INPUT_SIZE,
                                           password, NULL, &archive, &archive_size);
    CHECK("compression succeeds", rc == 0);
    if (!rc) {
        CHECK("writer emits format 1.6 with AIT", archive_size >= 128 &&
              archive[6] == 1 && archive[7] == 6 &&
              memcmp(archive + archive_size - 40, "ZEND", 4) == 0);
        CHECK("verify succeeds", zuptsdk_verify(ctx, archive, archive_size, password, NULL) == 0);
        rc = zuptsdk_extract_buffer(ctx, archive, archive_size, password, NULL, &output, &output_size);
        CHECK("roundtrip content", rc == 0 && output_size == INPUT_SIZE && memcmp(input, output, INPUT_SIZE) == 0);
        zuptsdk_free(output); output = NULL; output_size = 0;
        CHECK("set output quota", zuptsdk_ctx_set_max_decompressed(ctx, INPUT_SIZE - 1) == 0);
        CHECK("quota reports TOO_LARGE and clears outputs",
              zuptsdk_extract_buffer(ctx, archive, archive_size, password, NULL, &output, &output_size) == ZUPTSDK_ERR_TOO_LARGE &&
              output == NULL && output_size == 0);
        CHECK("reset output quota", zuptsdk_ctx_set_max_decompressed(ctx, 0) == 0);
        archive[12] ^= 1;
        CHECK("header tampering rejected", zuptsdk_verify(ctx, archive, archive_size, password, NULL) != 0);
        archive[12] ^= 1;
        CHECK("missing AIT rejected by default", zuptsdk_verify(ctx, archive, archive_size - 32, password, NULL) != 0);
        if (encrypted && dedup) {
            size_t first_ref = 0, second_ref = 0, first_end = 0, second_end = 0;
            for (size_t offset = 64, next; (next = next_frame(archive, archive_size, offset)) != 0; offset = next) {
                if (archive[offset + 2] == ZUPT_BLOCK_DEDUP_REF) {
                    if (!first_ref) { first_ref = offset; first_end = next; }
                    else { second_ref = offset; second_end = next; break; }
                }
            }
            CHECK("encrypted duplicate blocks use authenticated references", first_ref && second_ref &&
                  (zupt_le32_get(archive + 8) & ZUPT_FLAG_AUTH_DEDUP_REFS));
            if (first_ref && second_ref && first_end - first_ref == second_end - second_ref) {
                size_t frame_size = first_end - first_ref;
                for (size_t i = 0; i < frame_size; i++) {
                    uint8_t swap = archive[first_ref + i];
                    archive[first_ref + i] = archive[second_ref + i];
                    archive[second_ref + i] = swap;
                }
                CHECK("swapped authenticated dedup references rejected", zuptsdk_verify(ctx, archive, archive_size, password, NULL) != 0);
            }
        }
    }
    zuptsdk_free(output); zuptsdk_free(archive); free(input);
    zuptsdk_secure_buf_destroy(password); zuptsdk_options_destroy(opts); zuptsdk_ctx_destroy(ctx);
}

static void atomic_failures(void) {
    char directory[] = "/tmp/libvuptsdk-engine.XXXXXX";
    CHECK("make private test directory", mkdtemp(directory) != NULL);
    char archive_path[256], source_path[256], output_path[256];
    snprintf(archive_path, sizeof(archive_path), "%s/atomic.zupt", directory);
    snprintf(source_path, sizeof(source_path), "%s/missing", directory);
    snprintf(output_path, sizeof(output_path), "%s/payload.bin", directory);
    const char *names[] = {"payload.bin"}, *sources[] = {source_path};
    zupt_options_t options;
    zupt_default_options(&options); options.quiet = 1;
    CHECK("write existing archive", write_file(archive_path, "preserved", 9));
    CHECK("missing source compression fails", zupt_compress_files(archive_path, names, sources, 1, &options) != ZUPT_OK);
    CHECK("failed archive creation preserves destination", file_equals(archive_path, "preserved", 9));
    zuptsdk_options_t *sdk_options = NULL;
    uint8_t *archive = NULL;
    size_t archive_size = 0;
    CHECK("create store options", zuptsdk_options_create(&sdk_options) == 0 &&
          zuptsdk_options_set_codec(sdk_options, ZUPTSDK_CODEC_STORE) == 0);
    int rc = zuptsdk_compress_buffer(NULL, sdk_options, "payload.bin", (const uint8_t *)"original payload", 16,
                                     NULL, NULL, &archive, &archive_size);
    CHECK("create corruption fixture", rc == 0);
    if (!rc) {
        /* The first data frame starts at 64: magic(2), type(1), codec(2),
         * flags(2), one-byte sizes(2), checksum(8), then the stored bytes. */
        CHECK("store fixture has expected payload", archive_size > 97 && memcmp(archive + 81, "original payload", 16) == 0);
        archive[81] ^= 1;
        CHECK("write existing extraction target", write_file(output_path, "preserved", 9));
        CHECK("corrupt payload extraction fails", zuptsdk_extract_to_dir(NULL, archive, archive_size, directory, NULL, NULL) != 0);
        CHECK("failed extraction preserves destination", file_equals(output_path, "preserved", 9));
    }
    zuptsdk_free(archive); zuptsdk_options_destroy(sdk_options);
    unlink(archive_path); unlink(output_path); rmdir(directory);
}

static void legacy_opt_in(void) {
    FILE *fixture = fopen("tests/fixtures/sdk-2.0.4-plain.zupt.hex", "r");
    uint8_t archive[1024];
    size_t size = 0;
    unsigned value;
    while (fixture && size < sizeof(archive) && fscanf(fixture, "%2x", &value) == 1)
        archive[size++] = (uint8_t)value;
    if (fixture) fclose(fixture);
    CHECK("load preserved SDK 2.0.4 fixture", size > 96 && archive[7] == 4);
    zuptsdk_ctx_t *ctx = NULL;
    uint8_t *output = NULL;
    size_t output_size = 0;
    CHECK("create legacy context", zuptsdk_ctx_create(&ctx) == 0);
    CHECK("legacy opt-in validates arguments",
          zuptsdk_ctx_set_allow_legacy_no_ait(NULL, 1) == ZUPTSDK_ERR_INVALID_ARG &&
          zuptsdk_ctx_set_allow_legacy_no_ait(ctx, -1) == ZUPTSDK_ERR_INVALID_ARG &&
          zuptsdk_ctx_set_allow_legacy_no_ait(ctx, 2) == ZUPTSDK_ERR_INVALID_ARG);
    CHECK("old SDK archive rejected by default", zuptsdk_verify(ctx, archive, size, NULL, NULL) != 0);
    CHECK("enable trusted legacy mode", zuptsdk_ctx_set_allow_legacy_no_ait(ctx, 1) == 0);
    CHECK("trusted old SDK archive verifies", zuptsdk_verify(ctx, archive, size, NULL, NULL) == 0);
    static const char expected[] = "libvuptsdk 2.0.4 trusted legacy fixture\n";
    int rc = zuptsdk_extract_buffer(ctx, archive, size, NULL, NULL, &output, &output_size);
    CHECK("trusted old SDK archive content", rc == 0 && output_size == sizeof(expected) - 1 &&
          memcmp(output, expected, sizeof(expected) - 1) == 0);
    zuptsdk_free(output);
    CHECK("disable trusted legacy mode", zuptsdk_ctx_set_allow_legacy_no_ait(ctx, 0) == 0 &&
          zuptsdk_verify(ctx, archive, size, NULL, NULL) != 0);

    uint8_t *current = NULL; size_t current_size = 0;
    rc = zuptsdk_compress_buffer(ctx, NULL, "current.txt", (const uint8_t *)expected,
                                  sizeof(expected) - 1, NULL, NULL, &current, &current_size);
    CHECK("make current integrity fixture", rc == 0);
    if (!rc) {
        current[current_size - 32] ^= 1;
        CHECK("legacy opt-in cannot bypass present AIT failure",
              zuptsdk_ctx_set_allow_legacy_no_ait(ctx, 1) == 0 &&
              zuptsdk_verify(ctx, current, current_size, NULL, NULL) != 0);
    }
    zuptsdk_free(current); zuptsdk_ctx_destroy(ctx);
}

static void cpu_dispatch(void) {
    zupt_cpu_features_t detected;
    zupt_detect_cpu(&detected);
    uint8_t input[8192]; memset(input, 'A', sizeof(input));
    uint8_t *archive = NULL; size_t size = 0;
    int rc = zuptsdk_compress_buffer(NULL, NULL, "cpu.txt", input, sizeof(input),
                                     NULL, NULL, &archive, &size);
    CHECK("default compression succeeds", rc == 0);
    if (!rc) {
        uint16_t expected = ZUPT_CODEC_ZUPT_LZHP;
#if defined(__x86_64__) || defined(_M_X64)
        if (detected.has_avx2) expected = ZUPT_CODEC_VAPTVUPT;
#elif defined(__aarch64__) && defined(__ARM_NEON)
        expected = ZUPT_CODEC_VAPTVUPT;
#endif
        CHECK("AUTO codec uses runtime CPU detection", size > 69 && zupt_le16_get(archive + 67) == expected);
    }
    zuptsdk_free(archive);
    static const uint8_t digest[32] = {
        0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
    };
    uint8_t hash[32], scalar[32], hardware[32];
    zupt_sha256((const uint8_t *)"abc", 3, hash);
    CHECK("SHA-256 known vector", memcmp(hash, digest, sizeof(hash)) == 0);
    /* No workers are running here. Exercise both dispatch branches on hosts
     * with SHA-NI while keeping the global dispatch state restored. */
    zupt_cpu_features_t saved = zupt_cpu;
    zupt_cpu.has_shani = 0;
    zupt_sha256(input, sizeof(input), scalar);
    zupt_cpu.has_shani = detected.has_shani;
    zupt_sha256(input, sizeof(input), hardware);
    zupt_cpu = saved;
    CHECK("scalar and supported SHA hardware agree", memcmp(scalar, hardware, sizeof(scalar)) == 0);
}

static void mlkem_validation(void) {
    uint8_t pk[MLKEM_PUBLICKEYBYTES], sk[MLKEM_SECRETKEYBYTES];
    uint8_t ct[MLKEM_CIPHERTEXTBYTES], first[32], second[32];
    int rc = zupt_mlkem768_keygen(pk, sk);
    CHECK("ML-KEM generated keys validate", rc == 0 &&
          zupt_mlkem768_check_ek(pk, sizeof(pk)) == 0 && zupt_mlkem768_check_dk(sk, sizeof(sk)) == 0);
    if (!rc) {
        CHECK("ML-KEM encapsulation/decapsulation", zupt_mlkem768_encaps(ct, first, pk) == 0 &&
              zupt_mlkem768_decaps(second, ct, sk) == 0 && memcmp(first, second, 32) == 0);
        ct[0] ^= 1;
        CHECK("ML-KEM implicit rejection", zupt_mlkem768_decaps(second, ct, sk) == 0 && memcmp(first, second, 32) != 0);
        pk[0] = 255; pk[1] |= 15;
        CHECK("preserve local ML-KEM modulus check", zupt_mlkem768_check_ek(pk, sizeof(pk)) != 0 &&
              zupt_mlkem768_encaps(ct, first, pk) != 0);
        sk[1152 + 1184] ^= 1;
        CHECK("preserve local ML-KEM key hash check", zupt_mlkem768_check_dk(sk, sizeof(sk)) != 0);
    }
    zupt_secure_wipe(sk, sizeof(sk));
}

static void execute_only_ancestors(void) {
    char root[] = "/tmp/libvuptsdk-search.XXXXXX";
    if (!mkdtemp(root)) { CHECK("create search-only fixture", 0); return; }
    char ancestor[256], child[256], source[256], archive_path[256];
    char extraction[256], locked[256], nested[256], result[256], link_path[256];
    char outside[256], escaped[256];
    snprintf(ancestor, sizeof(ancestor), "%s/search", root);
    snprintf(child, sizeof(child), "%s/search/writable", root);
    snprintf(source, sizeof(source), "%s/search/writable/source.txt", root);
    snprintf(archive_path, sizeof(archive_path), "%s/search/writable/output.zupt", root);
    snprintf(extraction, sizeof(extraction), "%s/search/writable/extracted", root);
    snprintf(locked, sizeof(locked), "%s/search/writable/extracted/locked", root);
    snprintf(nested, sizeof(nested), "%s/search/writable/extracted/locked/nested", root);
    snprintf(result, sizeof(result), "%s/search/writable/extracted/locked/nested/payload.bin", root);
    snprintf(link_path, sizeof(link_path), "%s/search/writable/extracted/link", root);
    snprintf(outside, sizeof(outside), "%s/outside", root);
    snprintf(escaped, sizeof(escaped), "%s/outside/escape.bin", root);
    static const uint8_t payload[] = "traverse without directory listing permission";
    int prepared = mkdir(ancestor, 0700) == 0 && mkdir(child, 0700) == 0 &&
                   mkdir(extraction, 0700) == 0 && mkdir(locked, 0700) == 0 &&
                   mkdir(nested, 0700) == 0 && mkdir(outside, 0700) == 0 &&
                   write_file(source, payload, sizeof(payload)) &&
                   chmod(ancestor, 0111) == 0 && chmod(locked, 0111) == 0;
    CHECK("prepare execute-only ancestors with writable descendants", prepared);
    uint8_t *archive = NULL, *file_archive = NULL;
    size_t archive_size = 0, file_archive_size = 0;
    if (prepared) {
        const char *names[] = {"source.txt"}, *sources[] = {source};
        zupt_options_t options;
        zupt_default_options(&options); options.quiet = 1;
        CHECK("archive publication traverses execute-only ancestor",
              zupt_compress_files(archive_path, names, sources, 1, &options) == ZUPT_OK);
        CHECK("public SDK reads input through execute-only ancestor",
              zuptsdk_compress_files(NULL, NULL, sources, 1, NULL, NULL,
                                    &file_archive, &file_archive_size) == ZUPTSDK_OK);
        int rc = zuptsdk_compress_buffer(NULL, NULL, "locked/nested/payload.bin", payload,
                                        sizeof(payload), NULL, NULL, &archive, &archive_size);
        CHECK("prepare nested extraction fixture", rc == ZUPTSDK_OK);
        if (!rc) {
            CHECK("extraction traverses execute-only root and entry ancestors",
                  zuptsdk_extract_to_dir(NULL, archive, archive_size, extraction, NULL, NULL) == ZUPTSDK_OK &&
                  file_equals(result, payload, sizeof(payload)));
        }
        zuptsdk_free(archive); archive = NULL; archive_size = 0;
        rc = zuptsdk_compress_buffer(NULL, NULL, "link/escape.bin", payload,
                                    sizeof(payload), NULL, NULL, &archive, &archive_size);
        CHECK("prepare symlink traversal fixture", rc == ZUPTSDK_OK && symlink(outside, link_path) == 0);
        if (!rc) {
            CHECK("search descriptors still reject archive entry symlinks",
                  zuptsdk_extract_to_dir(NULL, archive, archive_size, extraction, NULL, NULL) != ZUPTSDK_OK &&
                  access(escaped, F_OK) != 0);
        }
    }
    zuptsdk_free(archive); zuptsdk_free(file_archive);
    (void)chmod(ancestor, 0700); (void)chmod(locked, 0700);
    unlink(link_path); unlink(result); unlink(archive_path); unlink(source); unlink(escaped);
    rmdir(nested); rmdir(locked); rmdir(extraction); rmdir(child); rmdir(ancestor); rmdir(outside); rmdir(root);
}

int main(void) {
    cpu_dispatch();
    roundtrip(7, 0, 0, 0, 1);
    roundtrip(9, 0, 0, 0, 2);
    roundtrip(9, 1, 0, 0, 1);
    roundtrip(7, 0, 1, 1, 2);
    atomic_failures();
    legacy_opt_in();
    mlkem_validation();
    execute_only_ancestors();
    fprintf(stderr, "Engine integration: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
