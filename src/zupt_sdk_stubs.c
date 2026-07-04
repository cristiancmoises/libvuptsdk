/* zupt_sdk_stubs.c — minimal stubs for the from-source build of libvuptsdk.
 *
 * The real implementations of zupt_sdk_hybrid_encrypt_init,
 * zupt_sdk_hybrid_decrypt_init, and zupt_sdk_password_decrypt_init live in
 * the zupt CLI tree (src/zupt_crypto_sdk.c) where they call into the SDK's
 * easy_* API. That file is not part of libvuptsdk because it's CLI glue.
 *
 * For the from-source build of libvuptsdk-base.so (the "subset" library),
 * we provide stubs that return -1 ("SDK PQ mode not available"). Code paths
 * inside zupt_format.c that depend on these features will fail cleanly with
 * an error message. The canonical prebuilt libvuptsdk.so does not use this
 * file — its zupt_sdk_* functions come from the prebuilt binary directly.
 *
 * Downstream users who need the SDK PQ mode should link against the
 * canonical libvuptsdk.so (the prebuilt one) rather than libvuptsdk-base.so.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "zupt.h"
#include <stdio.h>
#include <stddef.h>

/* All three weak — overridden if zupt_crypto_sdk.c is also linked in. */
__attribute__((weak))
int zupt_sdk_hybrid_encrypt_init(zupt_keyring_t *kr, const char *pubkeyfile,
                                  uint8_t *enc_hdr_buf, size_t *enc_hdr_len) {
    (void)kr; (void)pubkeyfile; (void)enc_hdr_buf; (void)enc_hdr_len;
    fprintf(stderr, "libvuptsdk: SDK PQ encrypt unavailable in from-source build.\n"
                    "            Link against the canonical libvuptsdk.so for full PQ support.\n");
    return -1;
}

__attribute__((weak))
int zupt_sdk_hybrid_decrypt_init(zupt_keyring_t *kr, const char *privkeyfile,
                                  const uint8_t *enc_hdr_buf, size_t enc_hdr_len) {
    (void)kr; (void)privkeyfile; (void)enc_hdr_buf; (void)enc_hdr_len;
    fprintf(stderr, "libvuptsdk: SDK PQ decrypt unavailable in from-source build.\n"
                    "            Link against the canonical libvuptsdk.so for full PQ support.\n");
    return -1;
}

__attribute__((weak))
int zupt_sdk_password_encrypt_init(zupt_keyring_t *kr, const char *password,
                                    uint8_t *enc_hdr_buf, size_t *enc_hdr_len) {
    (void)kr; (void)password; (void)enc_hdr_buf; (void)enc_hdr_len;
    fprintf(stderr, "libvuptsdk: SDK password encrypt unavailable in from-source build.\n"
                    "            Link against the canonical libvuptsdk.so for full support.\n");
    return -1;
}

__attribute__((weak))
int zupt_sdk_password_decrypt_init(zupt_keyring_t *kr, const char *password,
                                    const uint8_t *enc_hdr_buf, size_t enc_hdr_len) {
    (void)kr; (void)password; (void)enc_hdr_buf; (void)enc_hdr_len;
    fprintf(stderr, "libvuptsdk: SDK password decrypt unavailable in from-source build.\n"
                    "            Link against the canonical libvuptsdk.so for full support.\n");
    return -1;
}
