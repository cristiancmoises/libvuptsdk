/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Key isolation test: verify that the SECRET portion of the private key
 * never appears in any ciphertext.
 *
 * Background: the libzuptsdk privkey file format embeds the public key
 * twice (once after the privkey header, once inside the FIPS 203 ML-KEM
 * dk_KEM structure as `ek_KEM`). Both occurrences are pubkey material,
 * not secret. We have to find and exclude them.
 *
 * Method:
 *   1. Generate a keypair, read both files.
 *   2. Identify all "true secret" windows: 8-byte windows of the privkey
 *      file that DO NOT appear anywhere in the pubkey file.
 *   3. For each true-secret window, scan 100 ciphertexts to verify it
 *      never appears in any of them.
 *
 * Pass criterion: 0 leaks of true-secret windows in ciphertexts.
 */
#define _DEFAULT_SOURCE 1
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define WINDOW_SZ 8
#define N_TESTS 100

static int contains_window(const uint8_t *haystack, size_t hay_len,
                            const uint8_t *needle_window, size_t window_sz) {
    if (hay_len < window_sz) return 0;
    for (size_t i = 0; i <= hay_len - window_sz; i++) {
        if (memcmp(haystack + i, needle_window, window_sz) == 0)
            return 1;
    }
    return 0;
}

static uint8_t *read_file(const char *path, size_t *out_sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    size_t sz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(sz);
    size_t r = fread(buf, 1, sz, f);
    (void)r;
    fclose(f);
    *out_sz = sz;
    return buf;
}

int main(void) {
    char pub[] = "/tmp/ki_pub_XXXXXX";  close(mkstemp(pub));
    char priv[] = "/tmp/ki_priv_XXXXXX"; close(mkstemp(priv));
    if (zuptsdk_easy_keygen(pub, priv) != 0) return 1;

    size_t pub_sz, priv_sz;
    uint8_t *pub_data = read_file(pub, &pub_sz);
    uint8_t *priv_data = read_file(priv, &priv_sz);
    if (!pub_data || !priv_data) return 2;

    printf("═══ Key isolation test ═══\n");
    printf("  Public key size:  %zu bytes\n", pub_sz);
    printf("  Private key size: %zu bytes\n", priv_sz);
    printf("  Window size:      %d bytes (%d-bit)\n", WINDOW_SZ, WINDOW_SZ * 8);
    printf("  Tests:            %d ciphertexts\n\n", N_TESTS);

    /* Collect TRUE secret windows: 8-byte slices of privkey that are NOT
       present anywhere in pubkey. These are the bytes an attacker must
       compute (KEM secret seed, X25519 scalar, etc.). */
    if (priv_sz < WINDOW_SZ) return 3;
    size_t total_windows = priv_sz - WINDOW_SZ + 1;
    size_t true_secrets = 0;
    uint8_t *is_true_secret = calloc(total_windows, 1);

    for (size_t i = 0; i < total_windows; i++) {
        if (!contains_window(pub_data, pub_sz, priv_data + i, WINDOW_SZ)) {
            is_true_secret[i] = 1;
            true_secrets++;
        }
    }
    printf("  Total privkey windows:        %zu\n", total_windows);
    printf("  Pubkey-equivalent (excluded): %zu (per FIPS 203)\n",
           total_windows - true_secrets);
    printf("  TRUE secret windows:          %zu\n\n", true_secrets);

    if (true_secrets == 0) {
        fprintf(stderr, "ERROR: privkey is 100%% pubkey-equivalent? Bug.\n");
        return 4;
    }

    /* Now scan 100 ciphertexts for any true-secret window. */
    int leaks = 0;
    for (int t = 0; t < N_TESTS; t++) {
        uint8_t plaintext[256];
        for (size_t i = 0; i < sizeof(plaintext); i++)
            plaintext[i] = (uint8_t)(t * 257 + i);

        uint8_t *blob = NULL; size_t blob_sz = 0;
        if (zuptsdk_easy_encrypt(pub, plaintext, sizeof(plaintext),
                                  &blob, &blob_sz) != 0) {
            fprintf(stderr, "  encrypt %d failed\n", t); continue;
        }

        for (size_t i = 0; i < total_windows; i++) {
            if (!is_true_secret[i]) continue;
            if (contains_window(blob, blob_sz, priv_data + i, WINDOW_SZ)) {
                leaks++;
                fprintf(stderr, "  LEAK: secret window at priv offset %zu "
                        "found in ciphertext %d\n", i, t);
            }
        }
        free(blob);
    }
    printf("  TRUE secret windows × ciphertexts checked: %zu × %d = %zu trials\n",
           true_secrets, N_TESTS, true_secrets * N_TESTS);
    printf("  Leaks detected: %d  %s\n",
           leaks, leaks == 0 ? "✓ PASS" : "✗ FAIL");

    free(pub_data); free(priv_data); free(is_true_secret);
    unlink(pub); unlink(priv);
    return leaks == 0 ? 0 : 1;
}
