#!/usr/bin/env python3
# Copyright 2026 Cristian Cezar Moisés
# SPDX-License-Identifier: Apache-2.0
"""Compile/run the pure Bend model; report end-to-end native CPU timings."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import statistics
import subprocess
import time


ROOT = Path(__file__).resolve().parent.parent


def capture(command):
    return subprocess.run(command, cwd=ROOT, check=True, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repeats', type=int, default=7)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.repeats < 3:
        parser.error('--repeats must be at least 3')

    capture(['bend', 'PROOF.bend'])
    binary = ROOT / 'build/formal/aggregation-bench'
    binary.parent.mkdir(parents=True, exist_ok=True)
    capture(['bend', 'formal/benchmark.bend', '-o', str(binary)])
    # Independent arithmetic oracle for the deterministic sequence 1..256.
    leaves = 1 << 20
    expected_bytes = (leaves // 256) * (256 * 257 // 2)
    expected = f'{leaves} {expected_bytes}'
    samples = {1: [], 24: []}

    def run(threads):
        started = time.perf_counter()
        result = capture([str(binary), '--threads', str(threads)])
        elapsed = time.perf_counter() - started
        if result.stdout.strip() != expected:
            raise RuntimeError(f'Incorrect model output: {result.stdout!r}; expected {expected!r}')
        return elapsed

    for threads in samples:
        run(threads)  # One unrecorded warmup per configuration.
    for repeat in range(args.repeats):
        # Alternate order to reduce systematic warm-cache/thermal bias.
        for threads in ((1, 24) if repeat % 2 == 0 else (24, 1)):
            samples[threads].append(run(threads))

    medians = {threads: statistics.median(values) for threads, values in samples.items()}
    report = {
        'scope': 'Pure Bend model; generation + reduction + process startup + output, not C SDK performance',
        'input': {'depth': 20, 'leaves': leaves, 'leaf_bytes': '(index % 256) + 1',
                  'expected_count': leaves, 'expected_bytes': expected_bytes},
        'bend': capture(['bend', '--version']).stdout.strip(),
        'clang': capture(['clang', '--version']).stdout.splitlines()[0],
        'platform': platform.platform(),
        'processor': platform.processor(),
        'cpu_only': True,
        'warmups_per_configuration': 1,
        'repeats': args.repeats,
        'seconds': {str(threads): {'samples': values, 'median': medians[threads]}
                    for threads, values in samples.items()},
        'one_thread_median_divided_by_24_thread_median': medians[1] / medians[24],
        'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                          for path in (ROOT / 'formal/aggregation.bend', ROOT / 'formal/benchmark.bend',
                                       ROOT / 'LAWS.bend', ROOT / 'PROOF.bend')},
    }
    encoded = json.dumps(report, indent=2) + '\n'
    if args.output:
        args.output.write_text(encoded)
    print(encoded, end='')


if __name__ == '__main__':
    main()
