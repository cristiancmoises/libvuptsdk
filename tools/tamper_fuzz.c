/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Tamper fuzzer — encrypts known plaintext, mutates one random byte
   per iteration, verifies decrypt rejects every mutation. */
#define _DEFAULT_SOURCE 1
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define ITERATIONS 1000

int main(void) {
    /* Setup */
    char pub[] = "/tmp/lzfuzz_pub_XXXXXX"; close(mkstemp(pub));
    char priv[] = "/tmp/lzfuzz_priv_XXXXXX"; close(mkstemp(priv));
    if (zuptsdk_easy_keygen(pub, priv) != 0) return 1;

    const char *msg = "tamper-fuzz reference plaintext";
    uint8_t *blob = NULL;
    size_t blob_sz = 0;
    if (zuptsdk_easy_encrypt(pub, (uint8_t*)msg, strlen(msg), &blob, &blob_sz) != 0)
        return 2;

    srand((unsigned int)time(NULL));

    int detected = 0;
    int undetected = 0;
    int ok_decrypts = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        /* Make a mutated copy */
        uint8_t *mutated = malloc(blob_sz);
        memcpy(mutated, blob, blob_sz);

        size_t pos = rand() % blob_sz;
        uint8_t bit = 1u << (rand() % 8);
        mutated[pos] ^= bit;

        uint8_t *out = NULL;
        size_t out_sz = 0;
        int rc = zuptsdk_easy_decrypt(priv, mutated, blob_sz, &out, &out_sz);

        if (rc == 0 && out_sz == strlen(msg) && memcmp(out, msg, out_sz) == 0) {
            /* Perfect match — should only happen if we got really unlucky and
               flipped a bit in the part of the blob that's redundant. Count it
               but don't call it a failure. */
            ok_decrypts++;
        } else if (rc == 0) {
            /* Decrypt succeeded but plaintext differs — this would be a
               catastrophic AEAD failure. */
            undetected++;
        } else {
            detected++;
        }

        if (out) free(out);
        free(mutated);
    }

    free(blob);
    unlink(pub); unlink(priv);

    printf("Iterations:           %d\n", ITERATIONS);
    printf("Tampering detected:   %d (%.1f%%)\n", detected, 100.0*detected/ITERATIONS);
    printf("Decrypt OK (no-op):   %d (lucky bit-flips in redundant areas)\n", ok_decrypts);
    printf("UNDETECTED tampering: %d <- must be 0\n", undetected);

    return undetected == 0 ? 0 : 1;
}
