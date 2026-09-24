#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Cristian Cezar Moisés
set -eu
cd "$(dirname "$0")"
PREFIX=${PREFIX:-/usr/local}
DESTDIR=${DESTDIR:-}
case "$PREFIX" in /*) ;; *) echo 'PREFIX must be absolute' >&2; exit 2 ;; esac
install -d "$DESTDIR$PREFIX/lib/pkgconfig" "$DESTDIR$PREFIX/include/libvuptsdk-base" \
    "$DESTDIR$PREFIX/share/doc/libvuptsdk-base"
install -m 0755 usr/lib/libvuptsdk-base.so.@VERSION@ "$DESTDIR$PREFIX/lib/"
install -m 0644 usr/lib/libvuptsdk-base.a "$DESTDIR$PREFIX/lib/"
install -m 0644 usr/include/libvuptsdk-base/zuptsdk.h "$DESTDIR$PREFIX/include/libvuptsdk-base/"
install -m 0644 usr/share/doc/libvuptsdk-base/* "$DESTDIR$PREFIX/share/doc/libvuptsdk-base/"
ln -sfn libvuptsdk-base.so.@VERSION@ "$DESTDIR$PREFIX/lib/libvuptsdk-base.so.2"
ln -sfn libvuptsdk-base.so.2 "$DESTDIR$PREFIX/lib/libvuptsdk-base.so"
cat > "$DESTDIR$PREFIX/lib/pkgconfig/vuptsdk-base.pc" <<EOF
prefix=$PREFIX
libdir=\${prefix}/lib
includedir=\${prefix}/include/libvuptsdk-base

Name: vuptsdk-base
Description: source-built VaptVupt and Zupt archive SDK
Version: @RELEASE@
Libs: -L\${libdir} -lvuptsdk-base
Libs.private: -lpthread -lm
Cflags: -I\${includedir}
EOF
echo "Installed libvuptsdk-base @RELEASE@ under $DESTDIR$PREFIX"
echo 'Run ldconfig if this prefix is configured in your system loader paths.'
