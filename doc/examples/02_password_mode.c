/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    const char *password = "correct horse battery staple";
    const char *secret = "Bank account: 12345-678";

    uint8_t *blob = NULL;
    size_t blob_sz = 0;
    int rc = zuptsdk_easy_encrypt_password(password,
        (const uint8_t *)secret, strlen(secret), &blob, &blob_sz);
    if (rc != 0) {
        fprintf(stderr, "Encrypt failed: %s\n", zuptsdk_strerror(rc));
        return 1;
    }
    printf("Encrypted: %zu bytes\n", blob_sz);

    uint8_t *plain = NULL;
    size_t plain_sz = 0;
    rc = zuptsdk_easy_decrypt_password(password, blob, blob_sz, &plain, &plain_sz);
    if (rc != 0) {
        fprintf(stderr, "Decrypt failed: %s\n", zuptsdk_strerror(rc));
        return 1;
    }
    
    printf("Recovered: %.*s\n", (int)plain_sz, plain);

    free(blob);
    free(plain);
    return 0;
}
