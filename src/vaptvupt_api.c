/*
 * VaptVupt — libvuptsdk Integration API Implementation
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2026 Cristian.
 *
 * SDK-COMPAT: thin wrapper over vv_compress/vv_decompress with defaults
 * chosen for the authenticated, independently checksummed SDK container.
 */

#include "vaptvupt_api.h"
#include "vaptvupt.h"

int64_t vvz_compress(const uint8_t *src, size_t src_len,
                     uint8_t *dst, size_t dst_cap, int level) {
    vv_options_t opts;
    vv_default_options(&opts);
    /* The SDK stores an XXH64 for every uncompressed block and optionally
     * authenticates the compressed payload with AEAD. A second checksum in
     * the nested VaptVupt frame adds work without adding coverage. */
    opts.checksum = 0;
    /* Keep frames readable by decoders from v2.46.5 by suppressing the
     * four-stream Huffman literal representation. */
    opts.compat_v246_5_decoder = 1;

    if (level <= 2) {
        opts.mode = VV_MODE_ULTRA_FAST;
    } else if (level <= 7) {
        opts.mode = VV_MODE_BALANCED;
    } else {
        opts.mode = VV_MODE_EXTREME;
    }

    /* Keep the established SDK wire contract. The outer archive format has
     * its own versioning and must not silently start emitting v2 tag blocks. */
    opts.format_v2 = 0;

    /* Auto window: let adaptive selection choose wlog */
    opts.window_log = 0;

    return vv_compress(src, src_len, dst, dst_cap, &opts);
}

int64_t vvz_decompress(const uint8_t *src, size_t src_len,
                       uint8_t *dst, size_t dst_cap) {
    /* The caller verifies the archive block checksum after decompression;
     * encrypted blocks are authenticated before they reach this function. */
    return vv_decompress_flags(src, src_len, dst, dst_cap,
                               VV_DECOMPRESS_SKIP_CHECKSUM);
}

size_t vvz_compress_bound(size_t src_len) {
    return vv_compress_bound(src_len);
}

/* ═══════════════════════════════════════════════════════════════
 * Frame metadata accessor
 * ═══════════════════════════════════════════════════════════════ */

int vv_get_frame_info(const uint8_t *src, size_t src_len,
                      vv_frame_info_t *info) {
    if (!src || !info) return VV_ERR_PARAM;
    if (src_len < sizeof(vv_frame_header_t)) return VV_ERR_CORRUPT;

    vv_frame_header_t fh;
    memcpy(&fh, src, sizeof(fh));
    if (fh.magic != VV_MAGIC) return VV_ERR_BAD_MAGIC;
    if (fh.version != 1) return VV_ERR_CORRUPT;
    if ((fh.flags & 0x0Cu) == 0x0Cu) return VV_ERR_CORRUPT;
    if (fh.window_log < 10 || fh.window_log > 24) return VV_ERR_CORRUPT;

    info->version = fh.version;
    info->has_checksum = (fh.flags & 1) ? 1 : 0;
    info->mode_hint = fh.mode_hint;
    info->window_log = fh.window_log;
    info->content_size = fh.content_size;
    return VV_OK;
}
