#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Cristian Cezar Moisés
# Build libvuptsdk SRPM tarball.
# Produces: /tmp/libvuptsdk-2.0.0.srpm.tar.gz containing:
#           SPECS/libvuptsdk.spec  + SOURCES/libvuptsdk-2.0.0.tar.gz
# 
# To build the actual RPM on a system with rpmbuild:
#   tar xzf libvuptsdk-2.0.0.srpm.tar.gz
#   rpmbuild -bb SPECS/libvuptsdk.spec
set -e
cd "$(dirname "$0")/.."

VERSION="${VERSION:-2.0.0}"
SOVERSION="${SOVERSION:-2}"

# 1. Build source tarball
make dist
SRC_TAR="dist/libvuptsdk-${VERSION}.tar.gz"

if [ ! -f "$SRC_TAR" ]; then
    echo "Source tarball not built: $SRC_TAR"; exit 1
fi

# 2. Generate spec file
SPEC_DIR=$(mktemp -d)
mkdir -p "$SPEC_DIR/SPECS" "$SPEC_DIR/SOURCES"

cat > "$SPEC_DIR/SPECS/libvuptsdk.spec" <<SPEC
%define version    ${VERSION}
%define soversion  ${SOVERSION}

Name:           libvuptsdk
Version:        %{version}
Release:        1%{?dist}
Summary:        Post-quantum hybrid cryptography library
License:        AGPLv3+
URL:            https://github.com/cristiancmoises/libvuptsdk
Source0:        libvuptsdk-%{version}.tar.gz

BuildRequires:  gcc make pkgconfig
Requires:       glibc

%description
libvuptsdk provides a stable C ABI for post-quantum hybrid encryption
(ML-KEM-768 + X25519), authenticated encryption (XChaCha20-Poly1305 or
AES-256-SIV), Argon2id password mode, and streaming AEAD.

%package devel
Summary:        Development files for libvuptsdk
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
Headers, static archive, pkg-config file, and development docs for
libvuptsdk. Install this to build applications against libvuptsdk.

%prep
%setup -q -n libvuptsdk-%{version}

%build
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot} PREFIX=/usr LIBDIR=/usr/%{_lib}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
/usr/%{_lib}/libvuptsdk.so.%{version}
/usr/%{_lib}/libvuptsdk.so.%{soversion}
%doc README.md CHANGELOG.md SECURITY.md
%license LICENSE

%files devel
/usr/%{_lib}/libvuptsdk.so
/usr/%{_lib}/libvuptsdk.a
/usr/%{_lib}/pkgconfig/vuptsdk.pc
/usr/include/zuptsdk.h
/usr/include/zuptsdk_easy.h
/usr/include/zuptsdk.hpp
/usr/include/zuptsdk_metrics.h
/usr/include/zsdk_aes256_gcm_siv.h
/usr/include/zsdk_aes256_siv.h
/usr/include/zsdk_argon2id.h
/usr/include/zsdk_blake2b.h
/usr/include/zsdk_hkdf.h
/usr/include/zsdk_xchacha20_poly1305.h

%changelog
* Wed Apr 29 2026 Cristian Cezar Moisés <zupt@riseup.net> - %{version}-1
- libvuptsdk split out as standalone repo
- Two-library build (source + canonical prebuilt)
- See CHANGELOG.md for details
SPEC

# 3. Copy source into SOURCES dir
cp "$SRC_TAR" "$SPEC_DIR/SOURCES/"

# 4. Build SRPM-equivalent tarball
SRPM_TAR="/tmp/libvuptsdk-${VERSION}.srpm.tar.gz"
cd "$SPEC_DIR" && tar -czf "$SRPM_TAR" SPECS SOURCES
rm -rf "$SPEC_DIR"

if command -v rpmbuild >/dev/null 2>&1; then
    echo "Building actual RPM with rpmbuild..."
    RPM_TOPDIR=$(mktemp -d)
    tar xzf "$SRPM_TAR" -C "$RPM_TOPDIR"
    rpmbuild --define "_topdir $RPM_TOPDIR" -bb "$RPM_TOPDIR/SPECS/libvuptsdk.spec"
    cp "$RPM_TOPDIR"/RPMS/x86_64/*.rpm /tmp/
    rm -rf "$RPM_TOPDIR"
    echo "Built RPM(s) in /tmp"
else
    echo "rpmbuild not available — SRPM-equivalent tarball at: $SRPM_TAR"
    echo "  Distros: tar -xzf $SRPM_TAR && rpmbuild -bb SPECS/libvuptsdk.spec"
fi
