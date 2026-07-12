/*
 * VaptVupt — Zupt Integration API Implementation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright 2026 Cristian.
 *
 * ZUPT-COMPAT: thin wrapper over vv_compress/vv_decompress with
 * backup-optimized defaults. Decode speed prioritized over encode.
 */

#include "vaptvupt_api.h"
#include "vaptvupt.h"

int64_t vvz_compress(const uint8_t *src, size_t src_len,
                     uint8_t *dst, size_t dst_cap, int level) {
    vv_options_t opts;
    vv_default_options(&opts);
    /* Zupt integration (ZUPT_INTEGRATION.md, VaptVupt 2.48.2):
     * checksum=0 — Zupt's outer AEAD/HMAC already authenticates the
     * compressed bytes; the XXH64 footer is redundant work (~10% encode). */
    opts.checksum = 0;
    /* Broaden decoder-version compatibility for long-lived backups: suppress
     * the newest literal format (lit_fmt=4, 4-stream Huffman) so output stays
     * readable by any v2.46.5+ decoder. NOTE: this does NOT gate lit_fmt=3
     * (single-stream Huffman, added after the canonical prebuilt's vintage) —
     * there is no option to, so v2.48.2-encoded text is not readable by the
     * frozen prebuilt decoder. Full source<->prebuilt archive interop requires
     * regenerating the prebuilt from this codec (see CHANGELOG / SECURITY.md).
     * Self-roundtrip within the source library is unaffected. */
    opts.compat_v246_5_decoder = 1;

    if (level <= 2) {
        opts.mode = VV_MODE_ULTRA_FAST;
    } else if (level <= 7) {
        opts.mode = VV_MODE_BALANCED;
    } else {
        opts.mode = VV_MODE_EXTREME;
    }

    /* format_v2 ('T' tag blocks) would give 4-7% better binary ratio, but it
     * requires a v2.33.0+ decoder. It is kept OFF here for two reasons:
     *   1) Cross-library interop: the canonical prebuilt library embeds an
     *      older decoder that cannot read v2 frames, so a source-produced v2
     *      archive would fail to extract via the prebuilt (a backup library
     *      must always be able to re-read what it wrote).
     *   2) format_v2 + ULTRA_FAST frames fail decode with VV_ERR_OVERFLOW in
     *      every codec version tested.
     * Re-enable (gated to BALANCED/EXTREME) once the prebuilt is regenerated
     * from a v2.33.0+ codec. */
    opts.format_v2 = 0;

    /* Auto window: let adaptive selection choose wlog */
    opts.window_log = 0;

    return vv_compress(src, src_len, dst, dst_cap, &opts);
}

int64_t vvz_decompress(const uint8_t *src, size_t src_len,
                       uint8_t *dst, size_t dst_cap) {
    /* Skip XXH64 verification — zupt's HMAC-SHA256 already authenticates */
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

    info->version = fh.version;
    info->has_checksum = (fh.flags & 1) ? 1 : 0;
    info->mode_hint = fh.mode_hint;
    info->window_log = fh.window_log;
    info->content_size = fh.content_size;
    return VV_OK;
}
