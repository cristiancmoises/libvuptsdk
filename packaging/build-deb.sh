#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-libvuptsdk-Commercial
# Copyright (c) 2026 Cristian Cezar Moisés
# Build source-derived libvuptsdk-base Debian runtime and development packages.
set -eu

cd "$(dirname "$0")/.."

VERSION=$(make -s printversion)
RELEASE=$(make -s printrelease)
SOVERSION=${SOVERSION:-2}
ARCH=${ARCH:-$(dpkg --print-architecture)}
MULTIARCH=${MULTIARCH:-$(dpkg-architecture -qDEB_HOST_MULTIARCH)}
DEB_VERSION=${DEB_VERSION:-${VERSION}~base1-1}
OUT_DIR=${OUT_DIR:-$(pwd)/dist/packages}
SOURCE_LIB=build/libvuptsdk-base.so.${VERSION}
SOURCE_STATIC=build/libvuptsdk-base.a

make base

if [ ! -f "$SOURCE_LIB" ] || [ ! -f "$SOURCE_STATIC" ]; then
    echo "error: source-built base libraries were not produced" >&2
    exit 1
fi
if readelf -d "$SOURCE_LIB" | grep -Eq '\((RPATH|RUNPATH)\)'; then
    echo "error: refusing to package a library with an RPATH or RUNPATH" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/libvuptsdk-deb.XXXXXXXX")
trap 'rm -rf -- "$WORK_DIR"' EXIT HUP INT TERM

RT_NAME=libvuptsdk-base${SOVERSION}
DEV_NAME=libvuptsdk-base-dev
RT_ROOT=$WORK_DIR/runtime
DEV_ROOT=$WORK_DIR/devel
RT_LIB=$RT_ROOT/usr/lib/$MULTIARCH
DEV_LIB=$DEV_ROOT/usr/lib/$MULTIARCH
RT_DOC=$RT_ROOT/usr/share/doc/$RT_NAME
DEV_DOC=$DEV_ROOT/usr/share/doc/$DEV_NAME

mkdir -p "$RT_ROOT/DEBIAN" "$RT_LIB" "$RT_DOC"
install -m 0755 "$SOURCE_LIB" "$RT_LIB/libvuptsdk-base.so.${VERSION}"
if [ "${STRIP_DEB:-1}" != 0 ]; then
    strip --strip-unneeded "$RT_LIB/libvuptsdk-base.so.${VERSION}"
fi
ln -s "libvuptsdk-base.so.${VERSION}" "$RT_LIB/libvuptsdk-base.so.${SOVERSION}"
install -m 0644 LICENSE "$RT_DOC/copyright"
install -m 0644 README.md README.pt-BR.md CHANGELOG.md SECURITY.md NOTICE \
    LICENSE-AGPL-3.0 LICENSE-GPL-3.0 LICENSE-COMMERCIAL "$RT_DOC/"

RT_SIZE=$(du -sk "$RT_ROOT" | cut -f1)
cat > "$RT_ROOT/DEBIAN/control" <<EOF
Package: $RT_NAME
Version: $DEB_VERSION
Section: libs
Priority: optional
Architecture: $ARCH
Multi-Arch: same
Depends: libc6 (>= 2.34), libgcc-s1
Maintainer: Cristian Cezar Moisés <zupt@riseup.net>
Installed-Size: $RT_SIZE
Homepage: https://git.securityops.co/cristiancmoises/libvuptsdk
Description: source-built VaptVupt archive SDK runtime
 libvuptsdk-base provides the reproducible archive API backed by the
 VaptVupt 2.65.11 codec. It is intentionally separate from the frozen
 full-ABI libvuptsdk 2.0.3 compatibility binary.
EOF
cat > "$RT_ROOT/DEBIAN/triggers" <<'EOF'
activate-noawait ldconfig
EOF

mkdir -p "$DEV_ROOT/DEBIAN" "$DEV_LIB/pkgconfig" \
    "$DEV_ROOT/usr/include/libvuptsdk-base" "$DEV_DOC"
ln -s "libvuptsdk-base.so.${SOVERSION}" "$DEV_LIB/libvuptsdk-base.so"
install -m 0644 "$SOURCE_STATIC" "$DEV_LIB/libvuptsdk-base.a"
install -m 0644 include/zuptsdk.h \
    "$DEV_ROOT/usr/include/libvuptsdk-base/zuptsdk.h"
sed -e 's|^prefix=.*|prefix=/usr|' \
    -e "s|^libdir=.*|libdir=/usr/lib/$MULTIARCH|" \
    -e 's|^includedir=.*|includedir=/usr/include/libvuptsdk-base|' \
    build/vuptsdk-base.pc > "$DEV_LIB/pkgconfig/vuptsdk-base.pc"
install -m 0644 LICENSE "$DEV_DOC/copyright"
install -m 0644 README.md README.pt-BR.md doc/API_REFERENCE.md \
    doc/API_REFERENCE.pt-BR.md doc/example.c "$DEV_DOC/"

DEV_SIZE=$(du -sk "$DEV_ROOT" | cut -f1)
cat > "$DEV_ROOT/DEBIAN/control" <<EOF
Package: $DEV_NAME
Version: $DEB_VERSION
Section: libdevel
Priority: optional
Architecture: $ARCH
Multi-Arch: same
Depends: $RT_NAME (= $DEB_VERSION)
Maintainer: Cristian Cezar Moisés <zupt@riseup.net>
Installed-Size: $DEV_SIZE
Homepage: https://git.securityops.co/cristiancmoises/libvuptsdk
Description: development files for the source-built VaptVupt archive SDK
 This package contains the supported public header, static library and
 pkg-config metadata for libvuptsdk-base $RELEASE.
EOF

RT_DEB=$OUT_DIR/${RT_NAME}_${DEB_VERSION}_${ARCH}.deb
DEV_DEB=$OUT_DIR/${DEV_NAME}_${DEB_VERSION}_${ARCH}.deb
dpkg-deb --root-owner-group --build "$RT_ROOT" "$RT_DEB" >/dev/null
dpkg-deb --root-owner-group --build "$DEV_ROOT" "$DEV_DEB" >/dev/null

echo "Built: $RT_DEB"
echo "Built: $DEV_DEB"
