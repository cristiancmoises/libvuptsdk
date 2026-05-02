/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Wrong-key fuzz: encrypt N times to N different recipients, decrypt with
   each pair (key_i, blob_j) — all i != j must reject. */
#define _DEFAULT_SOURCE 1
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define N_KEYPAIRS 50

int main(void) {
    char pubs[N_KEYPAIRS][32], privs[N_KEYPAIRS][32];
    uint8_t *blobs[N_KEYPAIRS];
    size_t blob_szs[N_KEYPAIRS];

    /* Setup */
    for (int i = 0; i < N_KEYPAIRS; i++) {
        snprintf(pubs[i], sizeof(pubs[i]), "/tmp/lzwk%02d.pub", i);
        snprintf(privs[i], sizeof(privs[i]), "/tmp/lzwk%02d.priv", i);
        if (zuptsdk_easy_keygen(pubs[i], privs[i]) != 0) return 1;

        const char *msg = "wrong-key fuzz target";
        if (zuptsdk_easy_encrypt(pubs[i], (uint8_t*)msg, strlen(msg),
                                  &blobs[i], &blob_szs[i]) != 0) return 2;
    }

    int correct = 0, wrong_rejected = 0, wrong_accepted = 0;
    for (int i = 0; i < N_KEYPAIRS; i++) {
        for (int j = 0; j < N_KEYPAIRS; j++) {
            uint8_t *out = NULL; size_t out_sz = 0;
            int rc = zuptsdk_easy_decrypt(privs[j], blobs[i], blob_szs[i],
                                           &out, &out_sz);
            if (i == j) {
                if (rc == 0) correct++;
                else fprintf(stderr, "ERROR: i=j=%d but decrypt failed\n", i);
            } else {
                if (rc != 0) wrong_rejected++;
                else { wrong_accepted++;
                    fprintf(stderr, "ERROR: privs[%d] decrypted blob[%d]\n", j, i);
                }
            }
            if (out) free(out);
        }
    }

    /* Cleanup */
    for (int i = 0; i < N_KEYPAIRS; i++) {
        free(blobs[i]);
        unlink(pubs[i]); unlink(privs[i]);
    }

    printf("Total trials:      %d\n", N_KEYPAIRS * N_KEYPAIRS);
    printf("Correct decrypts:  %d (matching key)\n", correct);
    printf("Wrong rejected:    %d (%.2f%%)\n", wrong_rejected,
           100.0 * wrong_rejected / (N_KEYPAIRS * (N_KEYPAIRS - 1)));
    printf("Wrong ACCEPTED:    %d <- must be 0\n", wrong_accepted);
    return wrong_accepted == 0 ? 0 : 1;
}
