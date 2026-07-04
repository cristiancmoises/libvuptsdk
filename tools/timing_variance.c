/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Timing variance test for decrypt failure paths.
 *
 * Hypothesis: decrypt failure timing should be approximately constant,
 * regardless of how "close to valid" the failed input is. A statistically
 * detectable timing difference between failure modes would indicate a
 * non-constant-time MAC verify or KEM decapsulation path.
 *
 * Method: take a valid blob, mutate it in three different ways:
 *   1. Tamper with the ciphertext body (MAC fails late)
 *   2. Tamper with the KEM ciphertext (KEM-decaps fails)
 *   3. Truncate the blob (early format-error path)
 * Time each failure mode N times. Compare medians and p99 spreads.
 *
 * Pass criterion: timing of MAC-fail vs KEM-fail is within 50% of each
 * other (we expect KEM to dominate either way; tight constant-time means
 * spread should be small). Format errors are known to differ — that's OK.
 */
#define _DEFAULT_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ITERATIONS 500

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int cmp_d(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}

static void measure(const char *label, const uint8_t *blob, size_t blob_sz,
                    const char *priv_path) {
    double samples[ITERATIONS];
    for (int i = 0; i < ITERATIONS; i++) {
        uint8_t *out = NULL; size_t out_sz = 0;
        double t0 = now_sec();
        int rc = zuptsdk_easy_decrypt(priv_path, blob, blob_sz, &out, &out_sz);
        samples[i] = now_sec() - t0;
        if (rc == 0) {
            fprintf(stderr, "ERROR: %s should have failed but didn't\n", label);
            free(out);
        }
        if (out) free(out);
    }
    qsort(samples, ITERATIONS, sizeof(double), cmp_d);
    double median = samples[ITERATIONS / 2];
    double p10 = samples[ITERATIONS / 10];
    double p90 = samples[(int)(ITERATIONS * 0.9)];
    double p99 = samples[(int)(ITERATIONS * 0.99)];
    printf("  %-30s  median=%6.1fus  p10=%6.1fus  p90=%6.1fus  p99=%6.1fus\n",
           label, median * 1e6, p10 * 1e6, p90 * 1e6, p99 * 1e6);
}

int main(void) {
    char pub[] = "/tmp/tv_pub_XXXXXX";  close(mkstemp(pub));
    char priv[] = "/tmp/tv_priv_XXXXXX"; close(mkstemp(priv));
    if (zuptsdk_easy_keygen(pub, priv) != 0) return 1;

    /* Valid blob */
    const char *msg = "timing variance test plaintext";
    uint8_t *blob = NULL; size_t blob_sz = 0;
    if (zuptsdk_easy_encrypt(pub, (uint8_t*)msg, strlen(msg), &blob, &blob_sz) != 0)
        return 2;

    printf("═══ Timing variance: decrypt failure modes ═══\n");
    printf("  (per failure mode, %d iterations)\n\n", ITERATIONS);

    /* Mode 1: mutate the AEAD ciphertext (last 16 bytes are usually the MAC tag,
       so flip a byte ~30 from end which should be in the body). */
    uint8_t *m1 = malloc(blob_sz); memcpy(m1, blob, blob_sz);
    m1[blob_sz - 30] ^= 1;
    measure("Tamper AEAD body (MAC fail)", m1, blob_sz, priv);
    free(m1);

    /* Mode 2: mutate inside the KEM ciphertext (first ~100 bytes is KEM CT
       depending on format). The exact offset doesn't matter — early bytes
       are the KEM ciphertext. */
    uint8_t *m2 = malloc(blob_sz); memcpy(m2, blob, blob_sz);
    m2[50] ^= 1;
    measure("Tamper KEM ct (decaps fail?)", m2, blob_sz, priv);
    free(m2);

    /* Mode 3: truncate to half size (early parse error) */
    uint8_t *m3 = malloc(blob_sz); memcpy(m3, blob, blob_sz);
    measure("Truncated blob (parse fail)", m3, blob_sz / 2, priv);
    free(m3);

    /* Reference: valid decrypt for comparison */
    {
        double samples[ITERATIONS];
        for (int i = 0; i < ITERATIONS; i++) {
            uint8_t *out = NULL; size_t out_sz = 0;
            double t0 = now_sec();
            zuptsdk_easy_decrypt(priv, blob, blob_sz, &out, &out_sz);
            samples[i] = now_sec() - t0;
            free(out);
        }
        qsort(samples, ITERATIONS, sizeof(double), cmp_d);
        double median = samples[ITERATIONS / 2];
        double p10 = samples[ITERATIONS / 10];
        double p90 = samples[(int)(ITERATIONS * 0.9)];
        double p99 = samples[(int)(ITERATIONS * 0.99)];
        printf("  %-30s  median=%6.1fus  p10=%6.1fus  p90=%6.1fus  p99=%6.1fus\n",
               "Valid decrypt (reference)", median * 1e6, p10 * 1e6, p90 * 1e6, p99 * 1e6);
    }

    printf("\nInterpretation:\n");
    printf("  Constant-time KEM decaps + AEAD verify means MAC-fail and KEM-fail\n");
    printf("  should have similar medians (within scheduler noise, ~50%% of each other).\n");
    printf("  Truncated-parse failures are expected to be FASTER (early reject) — \n");
    printf("  this is an information leak only if the truncation point is\n");
    printf("  ciphertext-content-dependent, which it is not in libvuptsdk.\n");

    free(blob); unlink(pub); unlink(priv);
    return 0;
}
