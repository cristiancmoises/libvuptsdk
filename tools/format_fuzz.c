/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* Format fuzzer: feed completely malformed inputs to decrypt and assert
 * the library never crashes/leaks/hangs. Tests the parser robustness
 * against random byte strings.
 *
 * Pass criterion: every iteration completes without crash, ASAN error,
 * or hang. Decrypt should always return non-zero (we're feeding garbage).
 */
#define _DEFAULT_SOURCE 1
#include <zuptsdk.h>
#include <zuptsdk_easy.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define ITERATIONS 50000

static void fill_random(uint8_t *buf, size_t n) {
    for (size_t i = 0; i < n; i++)
        buf[i] = (uint8_t)(rand() & 0xFF);
}

int main(int argc, char *argv[]) {
    int iterations = ITERATIONS;
    if (argc > 1) iterations = atoi(argv[1]);

    char pub[] = "/tmp/ff_pub_XXXXXX";  close(mkstemp(pub));
    char priv[] = "/tmp/ff_priv_XXXXXX"; close(mkstemp(priv));
    if (zuptsdk_easy_keygen(pub, priv) != 0) return 1;

    srand(0xBEEF);

    int crashes = 0, errors = 0, accepts = 0;
    for (int i = 0; i < iterations; i++) {
        /* Random size 0 to 8192 bytes */
        size_t sz = (size_t)(rand() % 8192);
        uint8_t *buf = malloc(sz > 0 ? sz : 1);
        if (sz > 0) fill_random(buf, sz);

        uint8_t *out = NULL; size_t out_sz = 0;
        int rc = zuptsdk_easy_decrypt(priv, buf, sz, &out, &out_sz);

        if (rc == 0) {
            /* Random bytes accidentally decrypted? Should be cryptographically
               impossible — would be a catastrophic failure. */
            accepts++;
            fprintf(stderr, "ERROR: random %zu bytes decrypted at iter %d\n",
                    sz, i);
        } else {
            errors++;
        }

        if (out) free(out);
        free(buf);

        if (i > 0 && i % 10000 == 0)
            fprintf(stderr, "  ... %d / %d\r", i, iterations);
    }

    printf("\nIterations:   %d\n", iterations);
    printf("Errors:       %d (expected)\n", errors);
    printf("Accepts:      %d <- must be 0\n", accepts);
    printf("Crashes:      %d (would be detected by ASAN if rebuilt with sanitizer)\n",
           crashes);

    unlink(pub); unlink(priv);
    return accepts == 0 ? 0 : 1;
}
