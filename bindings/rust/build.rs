// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (c) 2026 Cristian Cezar Moisés
fn main() {
    pkg_config::Config::new()
        .atleast_version("2.0.0")
        .probe("zuptsdk")
        .expect("libzuptsdk-dev not found via pkg-config. Install with `make install` or use ZUPTSDK_LIB_DIR.");
}
