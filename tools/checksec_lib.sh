#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Cristian Cezar Moisés
#
# Comprehensive ELF hardening audit for libvuptsdk shared libraries.
# Reports on RELRO, NX, stack canary, FORTIFY_SOURCE, RPATH, symbol versions.
#
# Usage:
#   tools/checksec_lib.sh [path/to/library.so]
#   (default: prebuilt/libvuptsdk.so.2.0.0)
# Note: do NOT set -e — readelf | grep returns non-zero when a section
# is absent, which is normal control flow for this script.

LIB="${1:-prebuilt/libvuptsdk.so.2.0.0}"
if [ ! -f "$LIB" ]; then
    echo "Error: $LIB not found" >&2
    exit 1
fi

echo "═══ Hardening audit: $LIB ═══"
echo ""

FAIL=0
WARN=0

# 1. Type
TYPE=$(readelf -h "$LIB" | grep "Type:" | awk '{print $2}')
if [ "$TYPE" = "DYN" ]; then
    echo "ELF type:           $TYPE  ✓ PIE/PIC"
else
    echo "ELF type:           $TYPE  ✗ STATIC"
    FAIL=$((FAIL+1))
fi

# 2. Stack non-executable (NX bit)
GNU_STACK=$(readelf -l "$LIB" | grep "GNU_STACK" | grep -oE "RWE|RW |R E|RWX")
if echo "$GNU_STACK" | grep -q "E"; then
    echo "Executable stack:   ✗ FAIL - stack is executable"
    FAIL=$((FAIL+1))
else
    echo "Executable stack:   ✓ PASS - NX-enforced"
fi

# 3. RELRO (read-only relocations)
RELRO=$(readelf -l "$LIB" | grep "GNU_RELRO" | wc -l)
BIND_NOW=$(readelf -d "$LIB" | grep -E "BIND_NOW|FLAGS_1.*NOW" | wc -l)
if [ "$RELRO" -gt 0 ] && [ "$BIND_NOW" -gt 0 ]; then
    echo "RELRO:              ✓ PASS - Full RELRO (read-only GOT)"
elif [ "$RELRO" -gt 0 ]; then
    echo "RELRO:              ⚠ WARN - Partial RELRO (BIND_NOW missing)"
    WARN=$((WARN+1))
else
    echo "RELRO:              ✗ FAIL - No RELRO"
    FAIL=$((FAIL+1))
fi

# 4. Stack canaries
if nm -D "$LIB" 2>/dev/null | grep -q "__stack_chk_fail"; then
    echo "Stack canary:       ✓ PASS - canaries present"
else
    echo "Stack canary:       ✗ FAIL - no canary"
    FAIL=$((FAIL+1))
fi

# 5. FORTIFY_SOURCE
FORTIFIED=$(nm -D "$LIB" 2>/dev/null | grep -c "_chk@" || true)
if [ "$FORTIFIED" -gt 0 ]; then
    echo "FORTIFY_SOURCE:     ✓ PASS - $FORTIFIED _chk symbols"
else
    echo "FORTIFY_SOURCE:     ⚠ WARN - no _chk symbols"
    WARN=$((WARN+1))
fi

# 6. RPATH/RUNPATH
RPATH=$(readelf -d "$LIB" | grep -E "RPATH|RUNPATH" | head -1)
if [ -z "$RPATH" ]; then
    echo "RPATH/RUNPATH:      ✓ PASS - none set"
else
    echo "RPATH/RUNPATH:      ⚠ WARN - $RPATH"
    WARN=$((WARN+1))
fi

# 7. Symbol versioning
VERS_LIST=$(readelf --dyn-syms "$LIB" 2>/dev/null | grep -oE "@@?ZUPTSDK_[0-9.]+" | sort -u | tr '\n' ' ')
VERS=$(echo "$VERS_LIST" | wc -w)
echo "Symbol versions:    ✓ $VERS ABI versions ($VERS_LIST)"

# 8. SONAME
SONAME=$(readelf -d "$LIB" | grep "SONAME" | grep -oE "\[.*\]" | head -1)
echo "SONAME:             $SONAME"

# 9. Stripped? readelf-based: file(1) may be absent, and its failure must not
# silently fall through to the "stripped" (pass) branch.
if readelf -S "$LIB" 2>/dev/null | grep -qE '\.symtab|\.debug_info'; then
    echo "Debug info:         ⚠ NOTE - not stripped (debug info present)"
    WARN=$((WARN+1))
else
    echo "Debug info:         ✓ stripped"
fi

# 10. Dangerous symbols
DANGEROUS=""
for sym in gets _gets system popen execl execlp execle execv execvp execvpe; do
    if nm -D "$LIB" 2>/dev/null | grep -q " U $sym@" || nm "$LIB" 2>/dev/null | grep -q " T $sym$"; then
        DANGEROUS="$DANGEROUS $sym"
    fi
done
if [ -z "$DANGEROUS" ]; then
    echo "Dangerous symbols:  ✓ PASS - none of gets/system/exec* used"
else
    echo "Dangerous symbols:  ⚠ WARN -$DANGEROUS"
    WARN=$((WARN+1))
fi

# 11. Library size + symbol count
SIZE=$(stat -c %s "$LIB")
SYMS=$(nm -D "$LIB" 2>/dev/null | grep " T " | wc -l)
echo "Size:               $(numfmt --to=iec-i --suffix=B $SIZE)"
echo "Exported symbols:   $SYMS"

echo ""
echo "Dynamic dependencies:"
ldd "$LIB" 2>&1 | grep -v "linux-vdso\|/lib64/ld-linux" | sed 's/^/  /'

echo ""
echo "═══ Summary: $FAIL failures, $WARN warnings ═══"
exit $FAIL
