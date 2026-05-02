#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Cristian Cezar Moisés
# Build libzuptsdk Debian package.
# Produces: /tmp/libzuptsdk_2.0.0_amd64.deb
#           /tmp/libzuptsdk-dev_2.0.0_amd64.deb
set -e
cd "$(dirname "$0")/.."

VERSION="${VERSION:-2.0.0}"
SOVERSION="${SOVERSION:-2}"
ARCH="${ARCH:-amd64}"

# Build first if not already built
if [ ! -f "build/libzuptsdk.so.${VERSION}" ]; then
    make
fi

# ─── Runtime package: libzuptsdk2 ──────────────────────────────────
PKG_RT="/tmp/libzuptsdk${SOVERSION}_${VERSION}_${ARCH}"
rm -rf "$PKG_RT"
mkdir -p "$PKG_RT/DEBIAN" \
         "$PKG_RT/usr/lib/x86_64-linux-gnu" \
         "$PKG_RT/usr/share/doc/libzuptsdk${SOVERSION}"

install -m 0755 "build/libzuptsdk.so.${VERSION}" \
    "$PKG_RT/usr/lib/x86_64-linux-gnu/libzuptsdk.so.${VERSION}"
# Strip debug info to reduce package size and remove information disclosure.
# Override with STRIP_DEB=0 to keep symbols (debug builds).
if [ "${STRIP_DEB:-1}" != "0" ]; then
    strip --strip-unneeded \
        "$PKG_RT/usr/lib/x86_64-linux-gnu/libzuptsdk.so.${VERSION}" 2>/dev/null || true
fi
ln -sf "libzuptsdk.so.${VERSION}" \
    "$PKG_RT/usr/lib/x86_64-linux-gnu/libzuptsdk.so.${SOVERSION}"

install -m 0644 LICENSE       "$PKG_RT/usr/share/doc/libzuptsdk${SOVERSION}/copyright"
install -m 0644 README.md     "$PKG_RT/usr/share/doc/libzuptsdk${SOVERSION}/README.md"
install -m 0644 CHANGELOG.md  "$PKG_RT/usr/share/doc/libzuptsdk${SOVERSION}/CHANGELOG.md"
install -m 0644 SECURITY.md   "$PKG_RT/usr/share/doc/libzuptsdk${SOVERSION}/SECURITY.md"

INSTALLED_SIZE=$(du -sk "$PKG_RT" | cut -f1)
cat > "$PKG_RT/DEBIAN/control" <<EOF
Package: libzuptsdk${SOVERSION}
Version: ${VERSION}
Section: libs
Priority: optional
Architecture: ${ARCH}
Depends: libc6 (>= 2.28)
Maintainer: Cristian Cezar Moisés <zupt@riseup.net>
Installed-Size: ${INSTALLED_SIZE}
Homepage: https://github.com/cristiancmoises/libzuptsdk
Description: Post-quantum hybrid cryptography runtime library
 libzuptsdk provides a stable C ABI for post-quantum hybrid encryption
 (ML-KEM-768 + X25519), authenticated encryption (XChaCha20-Poly1305 or
 AES-256-SIV), Argon2id password mode, and streaming AEAD.
 .
 This package contains the runtime shared library only. For development
 headers, install libzuptsdk-dev.
EOF

# Triggers ldconfig
cat > "$PKG_RT/DEBIAN/postinst" <<'POST'
#!/bin/sh
set -e
ldconfig
POST
chmod 0755 "$PKG_RT/DEBIAN/postinst"

cat > "$PKG_RT/DEBIAN/postrm" <<'POST'
#!/bin/sh
set -e
ldconfig
POST
chmod 0755 "$PKG_RT/DEBIAN/postrm"

dpkg-deb --build "$PKG_RT" > /dev/null
echo "Built: ${PKG_RT}.deb"

# ─── Development package: libzuptsdk-dev ───────────────────────────
PKG_DEV="/tmp/libzuptsdk-dev_${VERSION}_${ARCH}"
rm -rf "$PKG_DEV"
mkdir -p "$PKG_DEV/DEBIAN" \
         "$PKG_DEV/usr/lib/x86_64-linux-gnu/pkgconfig" \
         "$PKG_DEV/usr/include" \
         "$PKG_DEV/usr/share/doc/libzuptsdk-dev"

# Symlink for -lzuptsdk
ln -sf "libzuptsdk.so.${SOVERSION}" \
    "$PKG_DEV/usr/lib/x86_64-linux-gnu/libzuptsdk.so"
# Static archive
install -m 0644 build/libzuptsdk-base.a \
    "$PKG_DEV/usr/lib/x86_64-linux-gnu/libzuptsdk.a"

# Public headers
for h in zuptsdk.h zuptsdk_easy.h zuptsdk.hpp zuptsdk_metrics.h \
         zsdk_aes256_gcm_siv.h zsdk_aes256_siv.h zsdk_argon2id.h \
         zsdk_blake2b.h zsdk_hkdf.h zsdk_xchacha20_poly1305.h; do
    install -m 0644 "include/$h" "$PKG_DEV/usr/include/"
done

# pkg-config — generate fresh with /usr prefix (Debian convention)
mkdir -p "$PKG_DEV/usr/lib/x86_64-linux-gnu/pkgconfig"
cat > "$PKG_DEV/usr/lib/x86_64-linux-gnu/pkgconfig/zuptsdk.pc" <<PCEOF
prefix=/usr
exec_prefix=\${prefix}
libdir=/usr/lib/x86_64-linux-gnu
includedir=/usr/include

Name: zuptsdk
Description: libzuptsdk - post-quantum hybrid cryptography
Version: ${VERSION}
Libs: -L\${libdir} -lzuptsdk
Cflags: -I\${includedir}
PCEOF
chmod 0644 "$PKG_DEV/usr/lib/x86_64-linux-gnu/pkgconfig/zuptsdk.pc"

install -m 0644 LICENSE       "$PKG_DEV/usr/share/doc/libzuptsdk-dev/copyright"
install -m 0644 README.md     "$PKG_DEV/usr/share/doc/libzuptsdk-dev/README.md"
install -m 0644 AUDIT.md      "$PKG_DEV/usr/share/doc/libzuptsdk-dev/AUDIT.md"

INSTALLED_SIZE_DEV=$(du -sk "$PKG_DEV" | cut -f1)
cat > "$PKG_DEV/DEBIAN/control" <<EOF
Package: libzuptsdk-dev
Version: ${VERSION}
Section: libdevel
Priority: optional
Architecture: ${ARCH}
Depends: libzuptsdk${SOVERSION} (= ${VERSION})
Maintainer: Cristian Cezar Moisés <zupt@riseup.net>
Installed-Size: ${INSTALLED_SIZE_DEV}
Homepage: https://github.com/cristiancmoises/libzuptsdk
Description: Post-quantum hybrid cryptography development files
 Headers, static archive, pkg-config file, and development docs for
 libzuptsdk. Install this to build applications against libzuptsdk.
 .
 Use 'pkg-config --cflags --libs zuptsdk' or '-lzuptsdk' to link.
EOF

dpkg-deb --build "$PKG_DEV" > /dev/null
echo "Built: ${PKG_DEV}.deb"
