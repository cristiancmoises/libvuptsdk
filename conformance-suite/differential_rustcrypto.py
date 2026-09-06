#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-libvuptsdk-Commercial
"""Bidirectional ML-KEM-768 differential against RustCrypto ml-kem 0.2.3."""

import pathlib
import subprocess
import sys
import tempfile


def run(*args, cwd=None):
    """Run one helper and return its whitespace-separated stdout."""
    return subprocess.run(
        args, cwd=cwd, check=True, text=True, capture_output=True
    ).stdout.split()


def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} KD_HELPER RUST_HELPER", file=sys.stderr)
        return 2

    kd = str(pathlib.Path(sys.argv[1]).resolve())
    rust = str(pathlib.Path(sys.argv[2]).resolve())
    sdk_to_rust = 0
    rust_to_sdk = 0
    iterations = 100

    with tempfile.TemporaryDirectory(prefix="libvuptsdk-rustdiff-") as temp:
        work = pathlib.Path(temp)

        for _ in range(iterations):
            public_key, private_key = run(kd, "keygen")
            sdk_public = work / "sdk-public.hex"
            sdk_public.write_text(public_key, encoding="ascii")
            run(rust, "encap", str(sdk_public), cwd=work)
            ciphertext = (work / "rk_ct.hex").read_text(encoding="ascii").strip()
            expected = (work / "rk_ss.hex").read_text(encoding="ascii").strip()
            sdk_to_rust += run(kd, "decap", private_key, ciphertext)[0] == expected

        for _ in range(iterations):
            run(rust, "keygen", cwd=work)
            public_key = (work / "rk_ek.hex").read_text(encoding="ascii").strip()
            ciphertext, expected = run(kd, "encap", public_key)
            sdk_ciphertext = work / "sdk-ciphertext.hex"
            sdk_ciphertext.write_text(ciphertext, encoding="ascii")
            actual = run(
                rust,
                "decap",
                str(work / "rk_dk.hex"),
                str(sdk_ciphertext),
                cwd=work,
            )[0]
            rust_to_sdk += actual == expected

    print(f"SDK->RustCrypto->SDK: {sdk_to_rust}/{iterations}")
    print(f"RustCrypto->SDK->RustCrypto: {rust_to_sdk}/{iterations}")
    return 0 if sdk_to_rust == rust_to_sdk == iterations else 1


if __name__ == "__main__":
    raise SystemExit(main())
