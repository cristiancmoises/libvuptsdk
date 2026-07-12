/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * dudect-style constant-time statistical test for zupt_mlkem768_decaps.
 *
 * Methodology (Reparaz, Balasch, Verbauwhede — "Dude, is my code constant
 * time?", DATE 2017): two input classes, randomly interleaved, timed with a
 * serialized cycle counter; Welch's t-test on the two timing distributions,
 * repeated under percentile crops (tail noise dominates on shared machines).
 *
 *   class 0: ONE fixed VALID ciphertext  (accept path, cmov selects K')
 *   class 1: fresh RANDOM invalid ciphertexts (implicit-rejection path, K̄)
 *
 * A constant-time decapsulation must show no statistically significant
 * timing difference between the classes.  |t| < 4.5 → no evidence of leakage
 * at this sample size;  |t| > 10 → definite leak (dudect conventions).
 *
 * Second experiment (-DEXP2): class 1 = fresh VALID ciphertexts, isolating
 * fixed-vs-random data effects on the accept path only.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86intrin.h>

int zupt_mlkem768_keygen(uint8_t pk[1184], uint8_t sk[2400]);
int zupt_mlkem768_encaps(uint8_t ct[1088], uint8_t ss[32], const uint8_t pk[1184]);
int zupt_mlkem768_decaps(uint8_t ss[32], const uint8_t ct[1088], const uint8_t sk[2400]);

static FILE *ur;
void zupt_random_bytes(uint8_t *b, size_t n) { fread(b, 1, n, ur); }

static inline uint64_t cyc(void) { _mm_lfence(); uint64_t t = __rdtsc(); _mm_lfence(); return t; }

#define N 60000            /* measurements (after warmup) */
#define WARM 600
#define CTLEN 1088

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* Welch's t on the subset of samples with time <= crop threshold */
static double welch_t(const uint64_t *t, const uint8_t *cls, int n, uint64_t crop,
                      int *n0o, int *n1o) {
    double m0 = 0, m1 = 0, s0 = 0, s1 = 0; int n0 = 0, n1 = 0;
    for (int i = 0; i < n; i++) {
        if (t[i] > crop) continue;
        double x = (double)t[i];
        if (cls[i] == 0) { n0++; double d = x - m0; m0 += d / n0; s0 += d * (x - m0); }
        else             { n1++; double d = x - m1; m1 += d / n1; s1 += d * (x - m1); }
    }
    if (n0 < 2 || n1 < 2) return 0.0;
    double v0 = s0 / (n0 - 1), v1 = s1 / (n1 - 1);
    *n0o = n0; *n1o = n1;
    return (m0 - m1) / __builtin_sqrt(v0 / n0 + v1 / n1);
}

int main(void) {
    ur = fopen("/dev/urandom", "rb");
    static uint8_t pk[1184], sk[2400], fixed_ct[CTLEN], ss[32];
    zupt_mlkem768_keygen(pk, sk);
    zupt_mlkem768_encaps(fixed_ct, ss, pk);

    /* Pre-generate everything OUTSIDE the timed region */
    static uint8_t cls[N + WARM];
    static uint64_t tim[N + WARM];
    uint8_t *inputs = malloc((size_t)(N + WARM) * CTLEN);
    if (!inputs) { fprintf(stderr, "oom\n"); return 2; }
    fread(cls, 1, N + WARM, ur);
    for (int i = 0; i < N + WARM; i++) {
        cls[i] &= 1;
        uint8_t *dst = inputs + (size_t)i * CTLEN;
        if (cls[i] == 0) memcpy(dst, fixed_ct, CTLEN);
        else {
#ifdef EXP2
            uint8_t s2[32]; zupt_mlkem768_encaps(dst, s2, pk);   /* fresh VALID */
#else
            fread(dst, 1, CTLEN, ur);                             /* random INVALID */
#endif
        }
    }

    /* Timed loop — nothing but decaps between the counter reads */
    for (int i = 0; i < N + WARM; i++) {
        const uint8_t *ct = inputs + (size_t)i * CTLEN;
        uint64_t t0 = cyc();
        zupt_mlkem768_decaps(ss, ct, sk);
        uint64_t t1 = cyc();
        tim[i] = t1 - t0;
    }

    /* Drop warmup */
    uint64_t *t = tim + WARM; uint8_t *c = cls + WARM;

    /* Crop thresholds at percentiles of the combined distribution */
    static uint64_t sorted[N];
    memcpy(sorted, t, sizeof(sorted));
    qsort(sorted, N, sizeof(uint64_t), cmp_u64);
    const double pcts[] = { 1.00, 0.99, 0.95, 0.90, 0.80, 0.70, 0.60, 0.50 };
    printf("%-6s %12s %8s %8s %10s\n", "crop", "threshold", "n0", "n1", "t");
    double max_abs_t = 0;
    for (unsigned k = 0; k < sizeof(pcts) / sizeof(pcts[0]); k++) {
        uint64_t thr = sorted[(int)(pcts[k] * (N - 1))];
        int n0 = 0, n1 = 0;
        double tv = welch_t(t, c, N, thr, &n0, &n1);
        if (tv < 0) tv = -tv;
        if (tv > max_abs_t) max_abs_t = tv;
        printf("p%-5.2f %12llu %8d %8d %10.3f\n",
               pcts[k], (unsigned long long)thr, n0, n1, tv);
    }
    printf("\nmax |t| = %.3f  →  %s\n", max_abs_t,
           max_abs_t < 4.5 ? "PASS: no evidence of timing leakage at this sample size"
           : max_abs_t < 10 ? "WARN: possible leakage — investigate"
                            : "FAIL: definite timing leak");
    /* Exit codes let the CI runner distinguish statistical noise from a real
     * leak: 0 = pass, 1 = WARN zone (advisory; shared runners are noisy),
     * 2 = definite leak (|t| >= 10) and a hard, blocking failure. */
    return max_abs_t < 4.5 ? 0 : (max_abs_t < 10 ? 1 : 2);
}
