// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Cristian Cezar Moisés
fn main() {
    pkg_config::Config::new()
        .atleast_version("2.0.0")
        .probe("vuptsdk")
        .expect("libvuptsdk-dev not found via pkg-config. Install with `make install` or use ZUPTSDK_LIB_DIR.");
}
