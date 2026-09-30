#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Cristian Cezar Moisés
# Build source-derived libvuptsdk-base RPM and source RPM packages.
set -eu

cd "$(dirname "$0")/.."

if ! command -v rpmbuild >/dev/null 2>&1; then
    echo "error: rpmbuild is required" >&2
    exit 2
fi

VERSION=$(make -s printversion)
RELEASE=$(make -s printrelease)
SOVERSION=${SOVERSION:-2}
RPM_RELEASE=${RPM_RELEASE:-0.2.base2}
RPMBUILD_FLAGS=${RPMBUILD_FLAGS:-}
OUT_DIR=${OUT_DIR:-$(pwd)/dist/packages}
DIST_NAME=libvuptsdk-base-${RELEASE}
SOURCE_ZUPT=dist/${DIST_NAME}-src.zupt

make dist
if [ ! -f "$SOURCE_ZUPT" ]; then
    echo "error: expected source archive was not produced: $SOURCE_ZUPT" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
TOPDIR=$(mktemp -d "${TMPDIR:-/tmp}/libvuptsdk-rpm.XXXXXXXX")
trap 'rm -rf -- "$TOPDIR"' EXIT HUP INT TERM
mkdir -p "$TOPDIR/BUILD" "$TOPDIR/BUILDROOT" "$TOPDIR/RPMS" \
    "$TOPDIR/SOURCES" "$TOPDIR/SPECS" "$TOPDIR/SRPMS"
cp "$SOURCE_ZUPT" "$TOPDIR/SOURCES/"

cat > "$TOPDIR/SPECS/libvuptsdk-base.spec" <<EOF
Name:           libvuptsdk-base
Version:        $VERSION
Release:        $RPM_RELEASE%{?dist}
Summary:        Source-built VaptVupt archive SDK
License:        Apache-2.0 AND BSD-2-Clause AND BSD-3-Clause AND CC0-1.0
URL:            https://git.securityops.co/cristiancmoises/libvuptsdk
Source0:        ${DIST_NAME}-src.zupt

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  python3
BuildRequires:  zupt >= 5.2.10

%description
libvuptsdk-base provides a reproducible archive API backed by the VaptVupt
2.65.13 codec. It is intentionally separate from the frozen full-ABI
libvuptsdk 2.0.3 compatibility binary.

%package devel
Summary:        Development files for libvuptsdk-base
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
The supported public header, static library and pkg-config metadata for
libvuptsdk-base $RELEASE.

%prep
%setup -q -c -T -n $DIST_NAME
zupt extract -o . %{SOURCE0}
cp -a $DIST_NAME/. .
rm -rf -- $DIST_NAME

%build
%make_build

%check
%make_build test

%install
%make_install PREFIX=%{_prefix} LIBDIR=%{_libdir} INCLUDEDIR=%{_includedir} STRIP_INSTALL=0

%files
%{_libdir}/libvuptsdk-base.so.$VERSION
%{_libdir}/libvuptsdk-base.so.$SOVERSION
%doc README.md README.pt-BR.md CHANGELOG.md SECURITY.md NOTICE
%license LICENSE LICENSE-BSD-2-Clause LICENSE-BSD-3-Clause LICENSE-CC0-1.0

%files devel
%{_libdir}/libvuptsdk-base.so
%{_libdir}/libvuptsdk-base.a
%{_libdir}/pkgconfig/vuptsdk-base.pc
%{_includedir}/libvuptsdk-base/zuptsdk.h
%doc doc/API_REFERENCE.md doc/API_REFERENCE.pt-BR.md doc/example.c

%changelog
* Wed Sep 30 2026 Cristian Cezar Moisés <zupt@riseup.net> - $VERSION-$RPM_RELEASE
- Publish the source-built base SDK with VaptVupt 2.65.13
EOF

# RPMBUILD_FLAGS=--nodeps is useful only when validating the recipe on a
# non-RPM host whose compiler is not registered in the RPM database.
# shellcheck disable=SC2086
rpmbuild $RPMBUILD_FLAGS --define "_topdir $TOPDIR" \
    --define "_smp_mflags -j2" -ba \
    "$TOPDIR/SPECS/libvuptsdk-base.spec"
find "$TOPDIR/RPMS" "$TOPDIR/SRPMS" -type f -name '*.rpm' \
    -exec cp {} "$OUT_DIR/" \;

echo "Built RPM assets in: $OUT_DIR"
