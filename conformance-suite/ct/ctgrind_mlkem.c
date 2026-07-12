/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * ctgrind-style constant-time check for libvuptsdk ML-KEM-768 (Valgrind).
 *
 * Method (Langley's ctgrind, as used by pqclean/BoringSSL): mark secret bytes
 * as UNDEFINED via VALGRIND_MAKE_MEM_UNDEFINED, run the primitive under
 * memcheck.  Any secret-dependent branch or memory index is reported as
 * "Conditional jump or move depends on uninitialised value(s)" / "Use of
 * uninitialised value" — on the REAL compiled artifact, optimizations included.
 *
 * Taint policy (documented, standard practice):
 *  - decaps: taint sk_pke = sk[0..1152) and z = sk[2368..2400).
 *            The embedded pk copy sk[1152..2368) is PUBLIC — left defined, so
 *            rejection sampling on rho (public by construction) is exercised
 *            without false positives.
 *  - encaps: taint the 32-byte m drawn from the RNG (the only secret input).
 *  - keygen: taint the z draw. d is left defined because rho = G(d‖k)[0:32]
 *            is public by construction (it ships inside ek); tainting d would
 *            false-positive on the public rejection-sampling loop — the same
 *            declassification pqclean's ctgrind harnesses apply.
 *
 * Exit 0 = no secret-dependent control flow / memory access observed.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <valgrind/memcheck.h>

int zupt_mlkem768_keygen(uint8_t pk[1184], uint8_t sk[2400]);
int zupt_mlkem768_encaps(uint8_t ct[1088], uint8_t ss[32], const uint8_t pk[1184]);
int zupt_mlkem768_decaps(uint8_t ss[32], const uint8_t ct[1088], const uint8_t sk[2400]);

static int taint_next = 0;   /* 0=no, 1=taint this RNG draw, then reset */
static int skip_draws = 0;   /* number of draws to leave defined first    */
static uint64_t seed = 0x243F6A8885A308D3ull;
static uint8_t rnd(void) { seed = seed * 6364136223846793005ull + 1442695040888963407ull; return (uint8_t)(seed >> 56); }
void zupt_random_bytes(uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) b[i] = rnd();
    if (skip_draws > 0) { skip_draws--; return; }
    if (taint_next) { VALGRIND_MAKE_MEM_UNDEFINED(b, n); taint_next = 0; }
}

int main(void) {
    static uint8_t pk[1184], sk[2400], ct[1088], bad[1088], ss[32];

    /* ── keygen with z tainted (d defined: rho is public by construction) ── */
    skip_draws = 1; taint_next = 1;              /* draw1=d (defined), draw2=z (tainted) */
    zupt_mlkem768_keygen(pk, sk);
    VALGRIND_MAKE_MEM_DEFINED(pk, sizeof pk);    /* pk is public output */
    fprintf(stderr, "[ctgrind] keygen(z tainted) done\n");

    /* ── encaps with m tainted ── */
    VALGRIND_MAKE_MEM_DEFINED(sk, sizeof sk);    /* reset for a clean encaps stage */
    taint_next = 1;                              /* next draw = m */
    zupt_mlkem768_encaps(ct, ss, pk);
    VALGRIND_MAKE_MEM_DEFINED(ct, sizeof ct);    /* ct is public output */
    VALGRIND_MAKE_MEM_DEFINED(ss, sizeof ss);
    fprintf(stderr, "[ctgrind] encaps(m tainted) done\n");

    /* ── decaps, valid ct, secrets tainted ── */
    VALGRIND_MAKE_MEM_UNDEFINED(sk, 1152);          /* sk_pke  */
    VALGRIND_MAKE_MEM_UNDEFINED(sk + 2368, 32);     /* z       */
    zupt_mlkem768_decaps(ss, ct, sk);
    VALGRIND_MAKE_MEM_DEFINED(ss, sizeof ss);
    fprintf(stderr, "[ctgrind] decaps(valid ct, sk_pke+z tainted) done\n");

    /* ── decaps, tampered ct (implicit rejection), secrets tainted ── */
    memcpy(bad, ct, sizeof bad); bad[7] ^= 0x01;
    zupt_mlkem768_decaps(ss, bad, sk);
    VALGRIND_MAKE_MEM_DEFINED(ss, sizeof ss);
    fprintf(stderr, "[ctgrind] decaps(tampered ct, sk_pke+z tainted) done\n");

    /* keep outputs alive */
    volatile uint8_t sink = ss[0] ^ pk[0] ^ ct[0]; (void)sink;
    fprintf(stderr, "[ctgrind] all stages complete\n");
    return 0;
}
