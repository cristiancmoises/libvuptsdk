/* Differential helper: real-RNG keygen/encap/decap over hex on the CLI.
 * Used by the bidirectional differential runners (vs kyber-py / RustCrypto).
 * All I/O and argument counts are checked so a malformed invocation exits
 * non-zero instead of emitting a wrong/empty hex line that a differential
 * runner might misread as agreement. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int zupt_mlkem768_keygen(uint8_t pk[1184], uint8_t sk[2400]);
int zupt_mlkem768_encaps(uint8_t ct[1088], uint8_t ss[32], const uint8_t pk[1184]);
int zupt_mlkem768_decaps(uint8_t ss[32], const uint8_t ct[1088], const uint8_t sk[2400]);

static FILE *ur;
void zupt_random_bytes(uint8_t *b, size_t n) {
    if (fread(b, 1, n, ur) != n) { fprintf(stderr, "kd: RNG read failed\n"); exit(2); }
}

static void put_hex(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) printf("%02x", p[i]);
    printf("\n");
}

/* Decode exactly `n` bytes of hex from `h`; exit(2) on wrong length. */
static void get_hex(const char *h, uint8_t *o, size_t n) {
    if (!h || strlen(h) != 2 * n) { fprintf(stderr, "kd: expected %zu hex bytes\n", n); exit(2); }
    for (size_t i = 0; i < n; i++)
        if (sscanf(h + 2 * i, "%2hhx", &o[i]) != 1) { fprintf(stderr, "kd: bad hex\n"); exit(2); }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: kd keygen | encap <ek> | decap <dk> <ct>\n"); return 2; }
    ur = fopen("/dev/urandom", "rb");
    if (!ur) { perror("kd: /dev/urandom"); return 2; }

    uint8_t pk[1184], sk[2400], ct[1088], ss[32];
    if (!strcmp(argv[1], "keygen")) {
        if (zupt_mlkem768_keygen(pk, sk) != 0) { fprintf(stderr, "kd: keygen failed\n"); return 1; }
        put_hex(pk, 1184);
        put_hex(sk, 2400);
    } else if (!strcmp(argv[1], "encap")) {
        if (argc < 3) { fprintf(stderr, "kd: encap needs <ek>\n"); return 2; }
        get_hex(argv[2], pk, 1184);
        if (zupt_mlkem768_encaps(ct, ss, pk) != 0) { fprintf(stderr, "kd: encaps rejected ek\n"); return 1; }
        put_hex(ct, 1088);
        put_hex(ss, 32);
    } else if (!strcmp(argv[1], "decap")) {
        if (argc < 4) { fprintf(stderr, "kd: decap needs <dk> <ct>\n"); return 2; }
        get_hex(argv[2], sk, 2400);
        get_hex(argv[3], ct, 1088);
        zupt_mlkem768_decaps(ss, ct, sk);
        put_hex(ss, 32);
    } else {
        fprintf(stderr, "kd: unknown mode '%s'\n", argv[1]);
        return 2;
    }
    return 0;
}
