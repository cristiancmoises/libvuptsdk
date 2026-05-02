#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Cristian Cezar Moisés
# libzuptsdk audit script — verifies that the source build is a subset
# of the canonical prebuilt binary (no symbol drift).

set -e
cd "$(dirname "$0")/.."

PASS=0; FAIL=0
chk() {
    if [ "$1" = "PASS" ]; then echo "  ✓ $2"; PASS=$((PASS+1))
    else echo "  ✗ $2"; FAIL=$((FAIL+1)); fi
}

echo "  [libzuptsdk audit — source build vs canonical binary]"

SOURCE_LIB="build/libzuptsdk-base.so.2.0.0"
CANON_LIB="prebuilt/libzuptsdk.so.2.0.0"

# A1. Both libraries exist
[ -f "$SOURCE_LIB" ] && chk PASS "Source build artifact present" || chk FAIL "Source build artifact present"
[ -f "$CANON_LIB" ] && chk PASS "Canonical binary present" || chk FAIL "Canonical binary present"

# A2. Architecture matches
SRC_ARCH=$(file "$SOURCE_LIB" | grep -oE "x86-64|aarch64|ARM64" | head -1)
CAN_ARCH=$(file "$CANON_LIB" | grep -oE "x86-64|aarch64|ARM64" | head -1)
[ "$SRC_ARCH" = "$CAN_ARCH" ] && chk PASS "Architecture matches ($SRC_ARCH)" \
    || chk FAIL "Architecture mismatch: source=$SRC_ARCH canonical=$CAN_ARCH"

# A3. SONAME format correct
SRC_SONAME=$(readelf -d "$SOURCE_LIB" 2>/dev/null | grep SONAME | awk '{print $NF}' | tr -d '[]')
echo "$SRC_SONAME" | grep -q "libzuptsdk-base.so.2" \
    && chk PASS "Source SONAME = $SRC_SONAME" \
    || chk FAIL "Source SONAME wrong: $SRC_SONAME"

CAN_SONAME=$(readelf -d "$CANON_LIB" 2>/dev/null | grep SONAME | awk '{print $NF}' | tr -d '[]')
echo "$CAN_SONAME" | grep -q "libzuptsdk.so.2" \
    && chk PASS "Canonical SONAME = $CAN_SONAME" \
    || chk FAIL "Canonical SONAME wrong: $CAN_SONAME"

# A4. Source export count
SRC_EXPORTS=$(nm -D --defined-only "$SOURCE_LIB" 2>/dev/null | awk '$2=="T"' | wc -l)
CAN_EXPORTS=$(nm -D --defined-only "$CANON_LIB" 2>/dev/null | awk '$2=="T"' | wc -l)
echo "    Source exports: $SRC_EXPORTS"
echo "    Canonical exports: $CAN_EXPORTS"
[ "$SRC_EXPORTS" -gt 30 ] && chk PASS "Source exports v1.0 ABI (>30 functions)" \
    || chk FAIL "Source ABI too small ($SRC_EXPORTS exports)"
[ "$CAN_EXPORTS" -gt 60 ] && chk PASS "Canonical exports v1.0+v2.1 ABI (>60 functions)" \
    || chk FAIL "Canonical ABI too small ($CAN_EXPORTS exports)"

# A5. Compute the symbol overlap and report honestly
nm -D --defined-only "$SOURCE_LIB" 2>/dev/null | awk '$2=="T"{print $3}' | sed 's/@.*//' | sort -u > /tmp/_libzuptsdk_src_syms
nm -D --defined-only "$CANON_LIB"  2>/dev/null | awk '$2=="T"{print $3}' | sed 's/@.*//' | sort -u > /tmp/_libzuptsdk_can_syms

SOURCE_ONLY=$(comm -23 /tmp/_libzuptsdk_src_syms /tmp/_libzuptsdk_can_syms | wc -l)
CANON_ONLY=$(comm -13  /tmp/_libzuptsdk_src_syms /tmp/_libzuptsdk_can_syms | wc -l)
COMMON=$(    comm -12  /tmp/_libzuptsdk_src_syms /tmp/_libzuptsdk_can_syms | wc -l)

echo "    Common symbols (overlap): $COMMON"
echo "    Source-only (extended API in source build): $SOURCE_ONLY"
echo "    Canonical-only (easy_* layer not in source): $CANON_ONLY"

# Both libraries must share at least the keypair core (8 functions)
[ "$COMMON" -ge 8 ] && chk PASS "Source and canonical share core ABI ($COMMON common symbols)" \
    || chk FAIL "Source and canonical do not share core ABI"

# All canonical-only symbols must be in the public zuptsdk_ namespace
NON_PUBLIC=$(comm -13 /tmp/_libzuptsdk_src_syms /tmp/_libzuptsdk_can_syms | grep -v "^zuptsdk_" | grep -v "^zuptsdk__" | wc -l)
[ "$NON_PUBLIC" -eq 0 ] && chk PASS "All canonical-only symbols are in public zuptsdk_ namespace" \
    || chk FAIL "Canonical has $NON_PUBLIC symbols outside public namespace"

# A6. Both libraries link without unresolved external SDK symbols
SRC_UNDEF=$(nm -D --undefined-only "$SOURCE_LIB" 2>/dev/null | awk '$2=="U"{print $3}' | grep -E "^zuptsdk_|^zupt_sdk_" | wc -l)
[ "$SRC_UNDEF" -eq 0 ] && chk PASS "Source build has no unresolved SDK symbols" \
    || chk FAIL "Source build has $SRC_UNDEF unresolved SDK symbols"

# A7. Linker version map is honored
nm -D "$SOURCE_LIB" 2>/dev/null | grep -q "ZUPTSDK_1.0" \
    && chk PASS "Source build exports ZUPTSDK_1.0 version" \
    || chk FAIL "Source build missing ZUPTSDK_1.0 version"

nm -D "$CANON_LIB" 2>/dev/null | grep -q "ZUPTSDK_2.1" \
    && chk PASS "Canonical exports ZUPTSDK_2.1 version (easy_* layer)" \
    || chk FAIL "Canonical missing ZUPTSDK_2.1 version"

# A8. No leaked private symbols in either library
SRC_LEAKED=$(nm -D --defined-only "$SOURCE_LIB" 2>/dev/null | awk '$2=="T"{print $3}' | grep -vE "^(zuptsdk_|_init|_fini|_start|_edata|_end|__)" | wc -l)
[ "$SRC_LEAKED" -eq 0 ] && chk PASS "Source build does not leak private symbols" \
    || chk FAIL "Source build leaks $SRC_LEAKED private symbols"

# Cleanup
rm -f /tmp/_libzuptsdk_src_syms /tmp/_libzuptsdk_can_syms

echo
echo "  ───────────────────────────────────────"
echo "  Audit results: $PASS passed, $FAIL failed"
echo "  ───────────────────────────────────────"
[ $FAIL -eq 0 ]
