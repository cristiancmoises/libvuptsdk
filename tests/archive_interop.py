#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Cristian Cezar Moisés
"""Exercise both archive directions against an independently built Zupt CLI."""
import argparse
import ctypes as c
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", default="build/libvuptsdk-base.so")
    parser.add_argument("--zupt", required=True)
    args = parser.parse_args()
    sdk = c.CDLL(str(Path(args.library).resolve()))
    cli = str(Path(args.zupt).resolve())
    pointer = c.c_void_p
    output_pointer = c.POINTER(pointer)
    size_pointer = c.POINTER(c.c_size_t)
    declarations = {
        "ctx_create": [output_pointer], "ctx_destroy": [pointer],
        "ctx_set_threads": [pointer, c.c_int],
        "options_create": [output_pointer], "options_destroy": [pointer],
        "options_set_level": [pointer, c.c_int],
        "options_set_solid": [pointer, c.c_int],
        "options_set_dedup": [pointer, c.c_int],
        "options_set_codec": [pointer, c.c_int],
        "options_set_block_size": [pointer, c.c_size_t],
        "secure_buf_from_data": [pointer, c.c_size_t, output_pointer],
        "secure_buf_destroy": [pointer],
        "keypair_generate": [pointer, output_pointer],
        "keypair_save_private": [pointer, c.c_char_p],
        "keypair_save_public": [pointer, c.c_char_p],
        "keypair_destroy": [pointer],
        "privkey_load": [c.c_char_p, output_pointer],
        "pubkey_load": [c.c_char_p, output_pointer],
        "privkey_destroy": [pointer], "pubkey_destroy": [pointer],
        "compress_buffer": [pointer, pointer, c.c_char_p, pointer, c.c_size_t,
                            pointer, pointer, output_pointer, size_pointer],
        "extract_buffer": [pointer, pointer, c.c_size_t, pointer, pointer,
                           output_pointer, size_pointer],
        "verify": [pointer, pointer, c.c_size_t, pointer, pointer],
        "free": [pointer],
    }
    for name, arguments in declarations.items():
        function = getattr(sdk, "zuptsdk_" + name)
        function.argtypes = arguments
        function.restype = None if name.endswith("destroy") or name == "free" else c.c_int

    def call(name, *arguments):
        result = getattr(sdk, "zuptsdk_" + name)(*arguments)
        if result not in (None, 0):
            raise AssertionError(f"SDK {name} failed: {result}")

    def command(*arguments, cwd):
        result = subprocess.run([cli, *map(str, arguments)], cwd=cwd,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=90, check=False)
        if result.returncode:
            raise AssertionError(f"Zupt {arguments[0]} failed ({result.returncode}): "
                                 + result.stderr.decode(errors="replace"))

    with tempfile.TemporaryDirectory(prefix="libvuptsdk-interop-") as temporary:
        root = Path(temporary)
        # Three identical blocks exercise actual dedup references, not merely
        # archives whose dedup flag is set but whose data has no duplicates.
        payload = bytes(i % 251 for i in range(65536)) * 3
        (root / "payload.bin").write_bytes(payload)
        context, password, pair, private, public = (pointer() for _ in range(5))
        call("ctx_create", c.byref(context))
        call("ctx_set_threads", context, 2)
        call("secure_buf_from_data", b"interop-test-password", 21, c.byref(password))
        call("keypair_generate", context, c.byref(pair))
        private_path, public_path = root / "private.key", root / "public.key"
        call("keypair_save_private", pair, bytes(private_path))
        call("keypair_save_public", pair, bytes(public_path))
        call("privkey_load", bytes(private_path), c.byref(private))
        call("pubkey_load", bytes(public_path), c.byref(public))
        pass_path = root / "password.txt"
        pass_path.write_text("interop-test-password\n")
        try:
            for name, level, solid, dedup, encryption in [
                ("default", 7, 0, 0, "none"),
                ("level9", 9, 0, 0, "none"),
                ("solid", 9, 1, 0, "none"),
                ("password-dedup", 7, 0, 1, "password"),
                ("hybrid-pq", 7, 0, 0, "pq"),
            ]:
                options, archive, output = pointer(), pointer(), pointer()
                archive_size, output_size = c.c_size_t(), c.c_size_t()
                call("options_create", c.byref(options))
                call("options_set_level", options, level)
                call("options_set_solid", options, solid)
                call("options_set_dedup", options, dedup)
                call("options_set_block_size", options, 65536)
                # The default case tests runtime AUTO; the others pin VaptVupt.
                if name != "default":
                    call("options_set_codec", options, 1)
                pw = password if encryption == "password" else None
                pk = public if encryption == "pq" else None
                sk = private if encryption == "pq" else None
                encode = ["--vv"] if name != "default" else []
                decode = []
                if encryption == "password":
                    encode += ["--kdf", "pbkdf2", "--pass-file", pass_path]
                    decode += ["--pass-file", pass_path]
                elif encryption == "pq":
                    encode += ["--pq", public_path]
                    decode += ["--pq", private_path]
                if solid:
                    encode += ["--solid"]
                if dedup:
                    encode += ["--dedup"]
                try:
                    call("compress_buffer", context, options, b"payload.bin", payload,
                         len(payload), pw, pk, c.byref(archive), c.byref(archive_size))
                    sdk_path = root / (name + "-sdk.zupt")
                    sdk_path.write_bytes(c.string_at(archive, archive_size.value))
                    command("test", *decode, sdk_path, cwd=root)
                    output_dir = root / (name + "-cli-output")
                    command("extract", "-t", 2, "-o", output_dir, *decode, sdk_path, cwd=root)
                    assert (output_dir / "payload.bin").read_bytes() == payload, name

                    cli_path = root / (name + "-cli.zupt")
                    command("compress", "-l", level, "-b", 65536, "-t", 2,
                            *encode, cli_path, "payload.bin", cwd=root)
                    data = cli_path.read_bytes()
                    call("verify", context, data, len(data), pw, sk)
                    call("extract_buffer", context, data, len(data), pw, sk,
                         c.byref(output), c.byref(output_size))
                    assert c.string_at(output, output_size.value) == payload, name
                    print(f"PASS {name}: SDK -> CLI and CLI -> SDK")
                finally:
                    call("free", archive)
                    call("free", output)
                    call("options_destroy", options)
        finally:
            call("privkey_destroy", private)
            call("pubkey_destroy", public)
            call("keypair_destroy", pair)
            call("secure_buf_destroy", password)
            call("ctx_destroy", context)
    print("Archive interoperability: 10 directions passed")


if __name__ == "__main__":
    main()
