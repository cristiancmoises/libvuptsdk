/* SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2026 Cristian Cezar Moisés
 * Shared comparison primitive for archive authentication and ML-KEM. */
#include "zupt.h"

/* Constant-time-intended buffer equality. Fixed-length OR accumulation has
 * no intended content-dependent branch or memory access. Compiled timing is
 * checked by the conformance harness; this is not a formal timing proof. */
int zupt_ct_memeq(const void *a, const void *b, size_t n) {
    const volatile uint8_t *pa = (const volatile uint8_t *)a;
    const volatile uint8_t *pb = (const volatile uint8_t *)b;
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++)
        diff |= (uint8_t)(pa[i] ^ pb[i]);
    return (int)((uint8_t)((((unsigned)diff) - 1u) >> 8) & 1u);
}
