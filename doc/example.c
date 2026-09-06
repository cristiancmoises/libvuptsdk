/*
 * libvuptsdk-base example: compress a buffer with VaptVupt, verify it,
 * then extract it.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-libvuptsdk-Commercial
 *
 * Build: cc example.c $(pkg-config --cflags --libs vuptsdk-base) -o example
 *        # or, before installation:
 *        cc example.c -Iinclude build/libvuptsdk-base.a -lpthread -lm \
 *            -o example
 */
#include <zuptsdk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int die(const char *what, int rc) {
    fprintf(stderr, "ERROR (%s): %s\n", what, zuptsdk_strerror(rc));
    fprintf(stderr, "  detail: %s\n", zuptsdk_last_error_detail());
    return 1;
}

int main(void) {
    int rc = zuptsdk_version_check(2, 0, 4);
    if (rc) return die("version_check", rc);
    printf("libvuptsdk %s\n\n", zuptsdk_version_string());

    zuptsdk_ctx_t *ctx = NULL;
    rc = zuptsdk_ctx_create(&ctx);
    if (rc) return die("ctx_create", rc);

    zuptsdk_options_t *opts = NULL;
    rc = zuptsdk_options_create(&opts);
    if (rc) { zuptsdk_ctx_destroy(ctx); return die("options_create", rc); }
    rc = zuptsdk_options_set_codec(opts, ZUPTSDK_CODEC_VAPTVUPT);
    if (rc) {
        zuptsdk_options_destroy(opts);
        zuptsdk_ctx_destroy(ctx);
        return die("set_codec", rc);
    }

    const char *plaintext = "This payload is compressed with VaptVupt.\n";
    uint8_t *archive = NULL;
    size_t archive_sz = 0;
    rc = zuptsdk_compress_buffer(ctx, opts,
                                 "payload.txt",
                                 (const uint8_t *)plaintext, strlen(plaintext),
                                 NULL, NULL,
                                 &archive, &archive_sz);
    if (rc) {
        zuptsdk_options_destroy(opts);
        zuptsdk_ctx_destroy(ctx);
        return die("compress_buffer", rc);
    }
    printf("Compressed %zu bytes -> %zu byte archive\n", strlen(plaintext), archive_sz);

    rc = zuptsdk_verify(ctx, archive, archive_sz, NULL, NULL);
    if (rc) {
        zuptsdk_free(archive);
        zuptsdk_options_destroy(opts);
        zuptsdk_ctx_destroy(ctx);
        return die("verify", rc);
    }

    uint8_t *extracted = NULL;
    size_t extracted_sz = 0;
    rc = zuptsdk_extract_buffer(ctx, archive, archive_sz,
                                NULL, NULL,
                                &extracted, &extracted_sz);
    if (rc) {
        zuptsdk_free(archive);
        zuptsdk_options_destroy(opts);
        zuptsdk_ctx_destroy(ctx);
        return die("extract_buffer", rc);
    }

    int ok = (extracted_sz == strlen(plaintext)) &&
             (memcmp(extracted, plaintext, extracted_sz) == 0);
    printf("Roundtrip: %s\n", ok ? "OK (byte-exact)" : "FAILED");
    printf("Extracted: %.*s", (int)extracted_sz, extracted);

    zuptsdk_free(extracted);
    zuptsdk_free(archive);
    zuptsdk_options_destroy(opts);
    zuptsdk_ctx_destroy(ctx);
    return ok ? 0 : 1;
}
