#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Cristian Cezar Moisés
# Build libvuptsdk Debian package.
# Produces: /tmp/libvuptsdk_2.0.0_amd64.deb
#           /tmp/libvuptsdk-dev_2.0.0_amd64.deb
set -e
cd "$(dirname "$0")/.."

# Derive the version from the Makefile (single source of truth) unless the
# caller overrides it, so packages can never be silently mislabeled on a bump.
VERSION="${VERSION:-$(make -s printversion)}"
SOVERSION="${SOVERSION:-2}"
ARCH="${ARCH:-amd64}"

# Build first if not already built. Pass VERSION through so an override that
# doesn't match the Makefile fails loudly here rather than at the install step.
if [ ! -f "build/libvuptsdk.so.${VERSION}" ]; then
    make
fi
if [ ! -f "build/libvuptsdk.so.${VERSION}" ]; then
    echo "error: build/libvuptsdk.so.${VERSION} not produced — VERSION=$VERSION does not match the Makefile build" >&2
    exit 1
fi

# ─── Runtime package: libvuptsdk2 ──────────────────────────────────
PKG_RT="/tmp/libvuptsdk${SOVERSION}_${VERSION}_${ARCH}"
rm -rf "$PKG_RT"
mkdir -p "$PKG_RT/DEBIAN" \
         "$PKG_RT/usr/lib/x86_64-linux-gnu" \
         "$PKG_RT/usr/share/doc/libvuptsdk${SOVERSION}"

install -m 0755 "build/libvuptsdk.so.${VERSION}" \
    "$PKG_RT/usr/lib/x86_64-linux-gnu/libvuptsdk.so.${VERSION}"
# Strip debug info to reduce package size and remove information disclosure.
# Override with STRIP_DEB=0 to keep symbols (debug builds).
if [ "${STRIP_DEB:-1}" != "0" ]; then
    strip --strip-unneeded \
        "$PKG_RT/usr/lib/x86_64-linux-gnu/libvuptsdk.so.${VERSION}" 2>/dev/null || true
fi
ln -sf "libvuptsdk.so.${VERSION}" \
    "$PKG_RT/usr/lib/x86_64-linux-gnu/libvuptsdk.so.${SOVERSION}"

install -m 0644 LICENSE       "$PKG_RT/usr/share/doc/libvuptsdk${SOVERSION}/copyright"
install -m 0644 README.md     "$PKG_RT/usr/share/doc/libvuptsdk${SOVERSION}/README.md"
install -m 0644 CHANGELOG.md  "$PKG_RT/usr/share/doc/libvuptsdk${SOVERSION}/CHANGELOG.md"
install -m 0644 SECURITY.md   "$PKG_RT/usr/share/doc/libvuptsdk${SOVERSION}/SECURITY.md"

INSTALLED_SIZE=$(du -sk "$PKG_RT" | cut -f1)
cat > "$PKG_RT/DEBIAN/control" <<EOF
Package: libvuptsdk${SOVERSION}
Version: ${VERSION}
Section: libs
Priority: optional
Architecture: ${ARCH}
Depends: libc6 (>= 2.28)
Maintainer: Cristian Cezar Moisés <zupt@riseup.net>
Installed-Size: ${INSTALLED_SIZE}
Homepage: https://git.securityops.co/cristiancmoises/libvuptsdk
Description: Post-quantum hybrid cryptography runtime library
 libvuptsdk provides a stable C ABI for post-quantum hybrid encryption
 (ML-KEM-768 + X25519), authenticated encryption (XChaCha20-Poly1305 or
 AES-256-SIV), Argon2id password mode, and streaming AEAD.
 .
 This package contains the runtime shared library only. For development
 headers, install libvuptsdk-dev.
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

# ─── Development package: libvuptsdk-dev ───────────────────────────
PKG_DEV="/tmp/libvuptsdk-dev_${VERSION}_${ARCH}"
rm -rf "$PKG_DEV"
mkdir -p "$PKG_DEV/DEBIAN" \
         "$PKG_DEV/usr/lib/x86_64-linux-gnu/pkgconfig" \
         "$PKG_DEV/usr/include" \
         "$PKG_DEV/usr/share/doc/libvuptsdk-dev"

# Symlink for -lvuptsdk
ln -sf "libvuptsdk.so.${SOVERSION}" \
    "$PKG_DEV/usr/lib/x86_64-linux-gnu/libvuptsdk.so"
# Static archive
install -m 0644 build/libvuptsdk-base.a \
    "$PKG_DEV/usr/lib/x86_64-linux-gnu/libvuptsdk.a"

# Public headers
for h in zuptsdk.h zuptsdk_easy.h zuptsdk.hpp zuptsdk_metrics.h \
         zsdk_aes256_gcm_siv.h zsdk_aes256_siv.h zsdk_argon2id.h \
         zsdk_blake2b.h zsdk_hkdf.h zsdk_xchacha20_poly1305.h; do
    install -m 0644 "include/$h" "$PKG_DEV/usr/include/"
done

# pkg-config — generate fresh with /usr prefix (Debian convention)
mkdir -p "$PKG_DEV/usr/lib/x86_64-linux-gnu/pkgconfig"
cat > "$PKG_DEV/usr/lib/x86_64-linux-gnu/pkgconfig/vuptsdk.pc" <<PCEOF
prefix=/usr
exec_prefix=\${prefix}
libdir=/usr/lib/x86_64-linux-gnu
includedir=/usr/include

Name: vuptsdk
Description: libvuptsdk - post-quantum hybrid cryptography
Version: ${VERSION}
Libs: -L\${libdir} -lvuptsdk
Cflags: -I\${includedir}
PCEOF
chmod 0644 "$PKG_DEV/usr/lib/x86_64-linux-gnu/pkgconfig/vuptsdk.pc"

install -m 0644 LICENSE       "$PKG_DEV/usr/share/doc/libvuptsdk-dev/copyright"
install -m 0644 README.md     "$PKG_DEV/usr/share/doc/libvuptsdk-dev/README.md"
install -m 0644 AUDIT.md      "$PKG_DEV/usr/share/doc/libvuptsdk-dev/AUDIT.md"

INSTALLED_SIZE_DEV=$(du -sk "$PKG_DEV" | cut -f1)
cat > "$PKG_DEV/DEBIAN/control" <<EOF
Package: libvuptsdk-dev
Version: ${VERSION}
Section: libdevel
Priority: optional
Architecture: ${ARCH}
Depends: libvuptsdk${SOVERSION} (= ${VERSION})
Maintainer: Cristian Cezar Moisés <zupt@riseup.net>
Installed-Size: ${INSTALLED_SIZE_DEV}
Homepage: https://git.securityops.co/cristiancmoises/libvuptsdk
Description: Post-quantum hybrid cryptography development files
 Headers, static archive, pkg-config file, and development docs for
 libvuptsdk. Install this to build applications against libvuptsdk.
 .
 Use 'pkg-config --cflags --libs vuptsdk' or '-lvuptsdk' to link.
EOF

dpkg-deb --build "$PKG_DEV" > /dev/null
echo "Built: ${PKG_DEV}.deb"
