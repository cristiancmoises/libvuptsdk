/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Focused checks for the VaptVupt core embedded by libvuptsdk.
 */

#define _POSIX_C_SOURCE 200809L

#include "vaptvupt.h"
#include "vaptvupt_api.h"
#include "zupt_parallel.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static unsigned failures;

static void check(int condition, const char *label)
{
    checks++;
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", label);
        failures++;
    }
}

static void fill_payload(uint8_t *buf, size_t size)
{
    uint32_t state = 0x6d2b79f5u;
    size_t i;

    for (i = 0; i < size; i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        if ((i & 255u) < 208u)
            buf[i] = (uint8_t)("VaptVupt SDK block "[i % 19u]);
        else
            buf[i] = (uint8_t)state;
    }
}

static int roundtrip_level(int level, vv_mode_t expected_mode)
{
    const size_t input_size = 16384;
    const size_t bound = vvz_compress_bound(input_size);
    uint8_t *input = malloc(input_size);
    uint8_t *encoded = malloc(bound);
    uint8_t *decoded = malloc(input_size);
    vv_frame_info_t info;
    int64_t encoded_size;
    int64_t decoded_size;

    if (!input || !encoded || !decoded) {
        free(input);
        free(encoded);
        free(decoded);
        fprintf(stderr, "allocation failed in level %d test\n", level);
        return -1;
    }

    fill_payload(input, input_size);
    encoded_size = vvz_compress(input, input_size, encoded, bound, level);
    check(encoded_size > 0, "wrapper compression succeeds");
    if (encoded_size <= 0)
        goto out;

    check(vv_get_frame_info(encoded, (size_t)encoded_size, &info) == VV_OK,
          "frame metadata is readable");
    check(info.content_size == input_size, "frame records the exact input size");
    check(info.mode_hint == (uint8_t)expected_mode, "level maps to expected mode");
    check(info.has_checksum == 0, "nested checksum remains disabled");

    decoded_size = vvz_decompress(encoded, (size_t)encoded_size,
                                  decoded, input_size);
    check(decoded_size == (int64_t)input_size, "wrapper decode returns exact size");
    check(decoded_size == (int64_t)input_size &&
              memcmp(input, decoded, input_size) == 0,
          "wrapper round-trip preserves bytes");
    check(vvz_decompress(encoded, (size_t)encoded_size,
                         decoded, input_size - 1) < 0,
          "undersized destination is rejected");

out:
    free(input);
    free(encoded);
    free(decoded);
    return 0;
}

static void test_encoder_capacity_contract(void)
{
    enum { CANARY_SIZE = 16 };
    const size_t input_size = 4096;
    const size_t bound = vvz_compress_bound(input_size);
    uint8_t *input = malloc(input_size);
    uint8_t *reference = malloc(bound);
    uint8_t *exact = NULL;
    uint8_t *limited = NULL;
    int64_t reference_size;
    int64_t result;
    size_t i;

    if (!input || !reference) {
        check(0, "capacity test fixture allocations succeed");
        goto out;
    }

    fill_payload(input, input_size);
    reference_size = vvz_compress(input, input_size, reference, bound, 5);
    check(reference_size > 1, "capacity reference compression succeeds");
    if (reference_size <= 1)
        goto out;

    exact = malloc((size_t)reference_size);
    limited = malloc((size_t)reference_size + CANARY_SIZE);
    if (!exact || !limited) {
        check(0, "exact-capacity output allocations succeed");
        goto out;
    }

    result = vvz_compress(input, input_size, exact,
                          (size_t)reference_size, 5);
    check(result == reference_size,
          "encoder accepts the exact produced capacity");
    check(result == reference_size &&
              memcmp(exact, reference, (size_t)reference_size) == 0,
          "exact-capacity encoding is byte-identical");

    memset(limited, 0xa5, (size_t)reference_size + CANARY_SIZE);
    result = vvz_compress(input, input_size, limited,
                          (size_t)reference_size - 1, 5);
    check(result == VV_ERR_OVERFLOW,
          "one-byte-short encoder capacity returns overflow");
    for (i = (size_t)reference_size - 1;
         i < (size_t)reference_size + CANARY_SIZE; i++) {
        if (limited[i] != 0xa5)
            break;
    }
    check(i == (size_t)reference_size + CANARY_SIZE,
          "one-byte-short encoding leaves the capacity canary intact");

out:
    free(input);
    free(reference);
    free(exact);
    free(limited);
}

static void test_truncation_and_metadata(void)
{
    const size_t input_size = 4096;
    const size_t bound = vvz_compress_bound(input_size);
    uint8_t *input = malloc(input_size);
    uint8_t *encoded = malloc(bound);
    uint8_t *mutated = malloc(bound);
    uint8_t *decoded = malloc(input_size);
    vv_frame_header_t header;
    vv_frame_info_t info;
    int64_t encoded_size;
    size_t cut;

    if (!input || !encoded || !mutated || !decoded) {
        check(0, "truncation test allocations succeed");
        goto out;
    }

    fill_payload(input, input_size);
    encoded_size = vvz_compress(input, input_size, encoded, bound, 2);
    check(encoded_size > 0, "truncation fixture compression succeeds");
    if (encoded_size <= 0)
        goto out;

    for (cut = 0; cut < (size_t)encoded_size; cut++) {
        int64_t rc = vvz_decompress(encoded, cut, decoded, input_size);
        if (rc >= 0) {
            check(0, "every truncated frame is rejected");
            break;
        }
    }
    if (cut == (size_t)encoded_size)
        check(1, "every truncated frame is rejected");

    memcpy(mutated, encoded, (size_t)encoded_size);
    memcpy(&header, mutated, sizeof(header));
    header.flags |= 0x0cu;
    memcpy(mutated, &header, sizeof(header));
    check(vv_get_frame_info(mutated, (size_t)encoded_size, &info) == VV_ERR_CORRUPT,
          "conflicting BCJ flags are rejected");
    check(vvz_decompress(mutated, (size_t)encoded_size,
                         decoded, input_size) == VV_ERR_CORRUPT,
          "decoder rejects conflicting BCJ flags");

    memcpy(mutated, encoded, (size_t)encoded_size);
    memcpy(&header, mutated, sizeof(header));
    header.window_log = 9;
    memcpy(mutated, &header, sizeof(header));
    check(vv_get_frame_info(mutated, (size_t)encoded_size, &info) == VV_ERR_CORRUPT,
          "out-of-range window is rejected");
    check(vvz_decompress(mutated, (size_t)encoded_size,
                         decoded, input_size) == VV_ERR_CORRUPT,
          "decoder rejects a window below the supported range");

    memcpy(mutated, encoded, (size_t)encoded_size);
    memcpy(&header, mutated, sizeof(header));
    header.window_log = 25;
    memcpy(mutated, &header, sizeof(header));
    check(vv_get_frame_info(mutated, (size_t)encoded_size, &info) == VV_ERR_CORRUPT,
          "oversized window metadata is rejected");
    check(vvz_decompress(mutated, (size_t)encoded_size,
                         decoded, input_size) == VV_ERR_CORRUPT,
          "decoder rejects a window above the supported range");

out:
    free(input);
    free(encoded);
    free(mutated);
    free(decoded);
}

static void test_checksum_contract(void)
{
    const size_t input_size = 8192;
    const size_t bound = vv_compress_bound(input_size);
    uint8_t *input = malloc(input_size);
    uint8_t *encoded = malloc(bound);
    uint8_t *decoded = malloc(input_size);
    vv_options_t options;
    int64_t encoded_size;

    if (!input || !encoded || !decoded) {
        check(0, "checksum test allocations succeed");
        goto out;
    }

    fill_payload(input, input_size);
    vv_default_options(&options);
    options.checksum = 1;
    encoded_size = vv_compress(input, input_size, encoded, bound, &options);
    check(encoded_size > (int64_t)sizeof(vv_frame_footer_t),
          "checksummed fixture compression succeeds");
    if (encoded_size <= (int64_t)sizeof(vv_frame_footer_t))
        goto out;

    encoded[(size_t)encoded_size - sizeof(vv_frame_footer_t)] ^= 0x80u;
    check(vv_decompress(encoded, (size_t)encoded_size,
                        decoded, input_size) == VV_ERR_CORRUPT,
          "core decoder detects a corrupted nested checksum");
    check(vvz_decompress(encoded, (size_t)encoded_size,
                         decoded, input_size) == (int64_t)input_size &&
              memcmp(input, decoded, input_size) == 0,
          "SDK wrapper delegates integrity to the outer block checksum");

out:
    free(input);
    free(encoded);
    free(decoded);
}

static void test_bcj_decode(void)
{
    const size_t input_size = 4096;
    const size_t bound = vv_compress_bound(input_size);
    uint8_t *input = calloc(1, input_size);
    uint8_t *encoded = malloc(bound);
    uint8_t *decoded = malloc(input_size);
    vv_options_t options;
    vv_frame_info_t info;
    int64_t encoded_size;
    size_t i;

    if (!input || !encoded || !decoded) {
        check(0, "BCJ test allocations succeed");
        goto out;
    }

    for (i = 0; i + 5 <= input_size; i += 16) {
        uint32_t displacement = (uint32_t)(int32_t)(1024 - (int32_t)(i + 5));
        input[i] = 0xe8u;
        input[i + 1] = (uint8_t)displacement;
        input[i + 2] = (uint8_t)(displacement >> 8);
        input[i + 3] = (uint8_t)(displacement >> 16);
        input[i + 4] = (uint8_t)(displacement >> 24);
    }

    vv_default_options(&options);
    options.filter_x86 = 1;
    encoded_size = vv_compress(input, input_size, encoded, bound, &options);
    check(encoded_size > 0, "x86 BCJ fixture compression succeeds");
    if (encoded_size <= 0)
        goto out;

    check(vv_get_frame_info(encoded, (size_t)encoded_size, &info) == VV_OK,
          "BCJ frame metadata is readable");
    check((encoded[5] & 0x04u) != 0, "x86 BCJ frame flag is present");
    check(vvz_decompress(encoded, (size_t)encoded_size,
                         decoded, input_size) == (int64_t)input_size &&
              memcmp(input, decoded, input_size) == 0,
          "SDK wrapper decodes a BCJ-filtered frame");

out:
    free(input);
    free(encoded);
    free(decoded);
}

static void test_parallel_codec_path(void)
{
    const size_t input_size = 32768;
    const size_t encoded_cap = vvz_compress_bound(input_size);
    uint8_t *input = malloc(input_size);
    uint8_t *encoded = malloc(encoded_cap);
    zpar_ctx_t *compress_ctx = NULL;
    zpar_ctx_t *decompress_ctx = NULL;
    zpar_slot_t *slot;
    vv_frame_info_t info;
    uint64_t checksum = 0;
    size_t encoded_size = 0;
    int slot_index;

    if (!input || !encoded) {
        check(0, "parallel test allocations succeed");
        goto out;
    }
    fill_payload(input, input_size);

    compress_ctx = zpar_create(2, (uint32_t)input_size, 0, NULL);
    check(compress_ctx != NULL, "parallel compression context is created");
    if (!compress_ctx)
        goto out;

    slot_index = zpar_submit_compress(compress_ctx, input, input_size, 0, 3,
                                      ZUPT_CODEC_VAPTVUPT);
    check(slot_index >= 0, "parallel VaptVupt block is submitted");
    if (slot_index < 0)
        goto out;

    slot = zpar_wait_slot(compress_ctx, slot_index);
    check(slot != NULL && slot->error == ZUPT_OK,
          "parallel VaptVupt compression succeeds");
    check(slot != NULL && slot->actual_codec == ZUPT_CODEC_VAPTVUPT,
          "parallel compression keeps the requested codec");
    if (!slot || slot->error != ZUPT_OK ||
        slot->actual_codec != ZUPT_CODEC_VAPTVUPT ||
        slot->output_len > encoded_cap)
        goto out;

    encoded_size = slot->output_len;
    checksum = slot->checksum;
    memcpy(encoded, slot->output, encoded_size);
    check(vv_get_frame_info(encoded, encoded_size, &info) == VV_OK,
          "parallel frame metadata is readable");
    check(info.mode_hint == VV_MODE_BALANCED,
          "parallel level 3 matches the serial balanced-mode policy");
    check(info.has_checksum == 0,
          "parallel frame omits the redundant nested checksum");
    zpar_release_slot(compress_ctx, slot_index);
    zpar_destroy(compress_ctx);
    compress_ctx = NULL;

    decompress_ctx = zpar_create(2, (uint32_t)input_size, 1, NULL);
    check(decompress_ctx != NULL, "parallel decompression context is created");
    if (!decompress_ctx)
        goto out;

    slot_index = zpar_submit_decompress(decompress_ctx, encoded, encoded_size,
                                        0, ZUPT_CODEC_VAPTVUPT, 0,
                                        checksum, input_size);
    check(slot_index >= 0, "parallel VaptVupt frame is submitted for decode");
    if (slot_index < 0)
        goto out;

    slot = zpar_wait_slot(decompress_ctx, slot_index);
    check(slot != NULL && slot->error == ZUPT_OK &&
              slot->output_len == input_size,
          "parallel VaptVupt decompression returns the exact size");
    check(slot != NULL && slot->error == ZUPT_OK &&
              slot->output_len == input_size &&
              memcmp(input, slot->output, input_size) == 0,
          "parallel VaptVupt round-trip preserves bytes");
    if (slot)
        zpar_release_slot(decompress_ctx, slot_index);

out:
    zpar_destroy(compress_ctx);
    zpar_destroy(decompress_ctx);
    free(input);
    free(encoded);
}

int main(void)
{
    check(VV_VERSION_MAJOR == 2 && VV_VERSION_MINOR == 65 &&
              VV_VERSION_PATCH == 11 &&
              strcmp(VV_VERSION_STRING, "2.65.11") == 0,
          "embedded codec reports version 2.65.11");
    check(vvz_compress_bound(0) >= sizeof(vv_frame_header_t),
          "zero-length input has a usable bound");

    if (roundtrip_level(1, VV_MODE_ULTRA_FAST) < 0 ||
        roundtrip_level(5, VV_MODE_BALANCED) < 0 ||
        roundtrip_level(9, VV_MODE_EXTREME) < 0)
        return 2;

    test_encoder_capacity_contract();
    test_truncation_and_metadata();
    test_checksum_contract();
    test_bcj_decode();
    test_parallel_codec_path();

    if (failures != 0) {
        fprintf(stderr, "codec integration: %u/%u checks failed\n",
                failures, checks);
        return 1;
    }

    printf("codec integration: %u checks passed\n", checks);
    return 0;
}
