/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Field-level encryption: derive key once, encrypt many DB columns. */
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    /* Step 1: at startup, derive a 32-byte master key from passphrase + salt */
    uint8_t salt[16];
    if (zuptsdk_easy_random_salt(salt) != 0) return 1;
    /* (in production, persist `salt` in your config; reuse for the same key) */

    uint8_t master_key[32];
    if (zuptsdk_easy_derive_key("master passphrase", salt, master_key) != 0) return 2;

    /* Step 2: encrypt many fields with this key */
    char *email_ct = NULL, *ssn_ct = NULL;
    if (zuptsdk_easy_encrypt_field(master_key, "alice@example.com", &email_ct) != 0) return 3;
    if (zuptsdk_easy_encrypt_field(master_key, "123-45-6789", &ssn_ct) != 0) return 4;

    printf("Email ciphertext (b64): %s\n", email_ct);
    printf("SSN ciphertext   (b64): %s\n", ssn_ct);

    /* Step 3: decrypt later */
    char *email_pt = NULL, *ssn_pt = NULL;
    if (zuptsdk_easy_decrypt_field(master_key, email_ct, &email_pt) != 0) return 5;
    if (zuptsdk_easy_decrypt_field(master_key, ssn_ct, &ssn_pt) != 0) return 6;

    printf("Recovered email: %s\n", email_pt);
    printf("Recovered SSN:   %s\n", ssn_pt);

    free(email_ct); free(ssn_ct); free(email_pt); free(ssn_pt);

    /* Wipe the master key from memory before exiting */
    zuptsdk_secure_zero(master_key, sizeof(master_key));
    return 0;
}
