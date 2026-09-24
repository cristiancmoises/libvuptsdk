#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Cristian Cezar Moisés
"""Measure the public base ABI, including temporary-file/archive overhead."""
import argparse
import ctypes as C
import json
from pathlib import Path
import platform
import statistics
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('library', type=Path)
    parser.add_argument('--iterations', type=int, default=5)
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error('iterations must be positive')
    lib = C.CDLL(str(args.library.resolve()))
    ptr, size = C.c_void_p, C.c_size_t
    signatures = {
        'zuptsdk_ctx_create': [C.POINTER(ptr)],
        'zuptsdk_ctx_set_threads': [ptr, C.c_int],
        'zuptsdk_options_create': [C.POINTER(ptr)],
        'zuptsdk_options_set_codec': [ptr, C.c_int],
        'zuptsdk_options_set_level': [ptr, C.c_int],
        'zuptsdk_compress_buffer': [ptr, ptr, C.c_char_p, ptr, size, ptr, ptr, C.POINTER(ptr), C.POINTER(size)],
        'zuptsdk_extract_buffer': [ptr, ptr, size, ptr, ptr, C.POINTER(ptr), C.POINTER(size)],
        'zuptsdk_free': [ptr],
        'zuptsdk_ctx_destroy': [ptr],
        'zuptsdk_options_destroy': [ptr],
    }
    for name, sig in signatures.items():
        getattr(lib, name).argtypes = sig
    lib.zuptsdk_version_string.restype = C.c_char_p

    def check(rc):
        if rc:
            raise RuntimeError(f'SDK error: {rc}')

    context, options = ptr(), ptr()
    check(lib.zuptsdk_ctx_create(C.byref(context)))
    check(lib.zuptsdk_ctx_set_threads(context, 1))
    check(lib.zuptsdk_options_create(C.byref(options)))
    check(lib.zuptsdk_options_set_codec(options, 1))
    results = []
    try:
        for length in (65536, 1048576):
            # Reproducible, moderately repetitive structured records, without
            # host files or downloaded benchmark inputs.
            pattern = b''.join(f'{i:04d}: SDK archive record; ação; value={i * 17:06d}\n'.encode() for i in range(1024))
            data = (pattern * (length // len(pattern) + 1))[:length]
            source = C.create_string_buffer(data)
            for level in (1, 9):
                check(lib.zuptsdk_options_set_level(options, level))
                compress_times, extract_times = [], []
                archive_size = 0
                for iteration in range(args.iterations + 1):
                    archive, archive_len, output, output_len = ptr(), size(), ptr(), size()
                    try:
                        begin = time.perf_counter()
                        check(lib.zuptsdk_compress_buffer(context, options, b'payload.bin', source, length,
                                                         None, None, C.byref(archive), C.byref(archive_len)))
                        compressed = time.perf_counter()
                        check(lib.zuptsdk_extract_buffer(context, archive, archive_len.value, None, None,
                                                       C.byref(output), C.byref(output_len)))
                        extracted = time.perf_counter()
                        if output_len.value != length or C.string_at(output, output_len.value) != data:
                            raise RuntimeError('archive round-trip mismatch')
                        archive_size = archive_len.value
                        if iteration:
                            compress_times.append(compressed - begin)
                            extract_times.append(extracted - compressed)
                    finally:
                        lib.zuptsdk_free(archive)
                        lib.zuptsdk_free(output)
                results.append({'input_bytes': length, 'level': level, 'archive_bytes': archive_size,
                                'compress_ms': statistics.median(compress_times) * 1000,
                                'extract_ms': statistics.median(extract_times) * 1000})
    finally:
        lib.zuptsdk_options_destroy(options)
        lib.zuptsdk_ctx_destroy(context)
    print(json.dumps({'sdk': lib.zuptsdk_version_string().decode(), 'system': platform.platform(),
                      'threads': 1, 'iterations': args.iterations, 'results': results}, indent=2))


if __name__ == '__main__':
    main()
