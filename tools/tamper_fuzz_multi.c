/* SPDX-License-Identifier: AGPL-3.0-or-later */
#define _DEFAULT_SOURCE 1
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define ITERATIONS 10000

int main(void) {
    char pub[] = "/tmp/lzfuzz2_pub_XXXXXX"; close(mkstemp(pub));
    char priv[] = "/tmp/lzfuzz2_priv_XXXXXX"; close(mkstemp(priv));
    if (zuptsdk_easy_keygen(pub, priv) != 0) return 1;

    const char *msg = "tamper-fuzz reference plaintext";
    uint8_t *blob = NULL;
    size_t blob_sz = 0;
    if (zuptsdk_easy_encrypt(pub, (uint8_t*)msg, strlen(msg), &blob, &blob_sz) != 0)
        return 2;

    srand(0xDEAD);

    int detected = 0, undetected = 0, ok_decrypts = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        uint8_t *mutated = malloc(blob_sz);
        memcpy(mutated, blob, blob_sz);
        /* Mutate up to 4 random bytes */
        int n_mut = (rand() % 4) + 1;
        for (int j = 0; j < n_mut; j++) {
            size_t pos = rand() % blob_sz;
            mutated[pos] ^= (uint8_t)(rand() & 0xFF);
        }

        uint8_t *out = NULL; size_t out_sz = 0;
        int rc = zuptsdk_easy_decrypt(priv, mutated, blob_sz, &out, &out_sz);

        if (rc == 0 && out_sz == strlen(msg) && memcmp(out, msg, out_sz) == 0) {
            ok_decrypts++;
        } else if (rc == 0) {
            undetected++;
            fprintf(stderr, "UNDETECTED at iter %d\n", i);
        } else {
            detected++;
        }
        if (out) free(out);
        free(mutated);
    }

    free(blob);
    unlink(pub); unlink(priv);

    printf("Iterations:           %d\n", ITERATIONS);
    printf("Tampering detected:   %d (%.2f%%)\n", detected, 100.0*detected/ITERATIONS);
    printf("Decrypt OK (no-op):   %d\n", ok_decrypts);
    printf("UNDETECTED tampering: %d <- must be 0\n", undetected);
    return undetected == 0 ? 0 : 1;
}
