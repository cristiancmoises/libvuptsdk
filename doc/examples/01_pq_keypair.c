/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  /* unlink */

int main(void) {
    /* Generate a keypair (one-time setup) */
    if (zuptsdk_easy_keygen("alice.pub", "alice.priv") != 0) {
        fprintf(stderr, "keygen failed\n");
        return 1;
    }

    /* Encrypt a message for Alice */
    const char *plaintext = "Top secret message";
    uint8_t *ciphertext = NULL;
    size_t ct_len = 0;

    int rc = zuptsdk_easy_encrypt("alice.pub",
                                   (const uint8_t *)plaintext,
                                   strlen(plaintext) + 1,
                                   &ciphertext, &ct_len);
    if (rc != 0) {
        fprintf(stderr, "Encrypt failed: %s\n", zuptsdk_strerror(rc));
        return 1;
    }

    /* Decrypt with Alice's private key */
    uint8_t *recovered = NULL;
    size_t rec_len = 0;
    rc = zuptsdk_easy_decrypt("alice.priv",
                               ciphertext, ct_len,
                               &recovered, &rec_len);
    if (rc != 0) {
        fprintf(stderr, "Decrypt failed: %s\n", zuptsdk_strerror(rc));
        return 1;
    }

    printf("Recovered: %s\n", (char *)recovered);

    free(ciphertext);
    free(recovered);
    unlink("alice.pub"); unlink("alice.priv");
    return 0;
}
