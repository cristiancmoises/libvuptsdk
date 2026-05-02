/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* File encryption — no size limit. */
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    /* Setup: keypair + sample file */
    if (zuptsdk_easy_keygen("test.pub", "test.priv") != 0) return 1;

    FILE *f = fopen("input.txt", "wb");
    fputs("Document contents to be encrypted on disk.\n", f);
    fclose(f);

    /* Encrypt input.txt → input.txt.enc */
    int rc = zuptsdk_easy_encrypt_file("test.pub", "input.txt", "input.txt.enc",
                                        NULL, NULL);
    if (rc != 0) {
        fprintf(stderr, "encrypt_file failed: %s\n", zuptsdk_strerror(rc));
        return 2;
    }
    printf("Encrypted file written\n");

    /* Decrypt input.txt.enc → output.txt */
    rc = zuptsdk_easy_decrypt_file("test.priv", "input.txt.enc", "output.txt",
                                    NULL, NULL);
    if (rc != 0) {
        fprintf(stderr, "decrypt_file failed: %s\n", zuptsdk_strerror(rc));
        return 3;
    }

    /* Verify */
    f = fopen("output.txt", "rb");
    char buf[256] = {0};
    size_t bytes_read = fread(buf, 1, sizeof(buf) - 1, f);
    (void)bytes_read;  /* silence unused-result */
    fclose(f);
    printf("Recovered: %s", buf);

    /* Cleanup */
    remove("test.pub"); remove("test.priv");
    remove("input.txt"); remove("input.txt.enc"); remove("output.txt");
    return 0;
}
