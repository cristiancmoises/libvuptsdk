#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Cristian Cezar Moisés
"""Create source or Linux SDK packages with Zupt's extreme compression mode."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import stat
import subprocess
import tempfile


def safe_path(value):
    path = PurePosixPath(value)
    if not value or path.is_absolute() or any(p in ('..', '.') for p in value.split('/')):
        raise ValueError(f'unsafe source path: {value!r}')
    return path


def source_files(root):
    """Export only Git-tracked files, or a verified extracted source manifest."""
    git_root = None
    if shutil.which('git'):
        git_root = subprocess.run(['git', '-C', str(root), 'rev-parse', '--show-toplevel'],
                                  capture_output=True, text=True)
    if git_root and git_root.returncode == 0 and Path(git_root.stdout.strip()).resolve() == root.resolve():
        names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode().split('\0')
        entries = [{'path': name} for name in names if name]
    else:
        entries = json.loads((root / 'SOURCE-MANIFEST.json').read_text())['files']
    seen = set()
    for entry in sorted(entries, key=lambda e: e['path']):
        rel = safe_path(entry['path'])
        if str(rel) in seen:
            raise ValueError(f'duplicate source path: {rel}')
        seen.add(str(rel))
        if rel.parts[0] in ('prebuilt', 'build', 'dist', 'release', '.git'):
            continue
        path = root / rel
        # Reject symlinks in any component, not just the final file.
        if any((root.joinpath(*rel.parts[:i])).is_symlink() for i in range(1, len(rel.parts) + 1)):
            raise ValueError(f'symlink in source path: {rel}')
        st = path.stat()
        if not stat.S_ISREG(st.st_mode):
            raise ValueError(f'non-regular source path: {rel}')
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if 'sha256' in entry and digest != entry['sha256']:
            raise ValueError(f'source manifest checksum mismatch: {rel}')
        mode = entry.get('mode', 0o755 if st.st_mode & 0o111 else 0o644)
        if mode not in (0o644, 0o755):
            raise ValueError(f'invalid source manifest mode: {rel}')
        yield path, str(rel), digest, mode


def stage_source(root, stage):
    entries = list(source_files(root))
    if not entries:
        raise ValueError('no tracked source files')
    stage.mkdir(parents=True)
    manifest = []
    epoch = int(os.environ.get('SOURCE_DATE_EPOCH', '0'))
    for source, rel, digest, mode in entries:
        target = stage / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(mode)
        os.utime(target, (epoch, epoch))
        manifest.append({'path': rel, 'sha256': digest, 'mode': mode,
                         'size': target.stat().st_size})
    (stage / 'SOURCE-MANIFEST.json').write_text(json.dumps({'files': manifest}, indent=2) + '\n')


def stage_binary(root, stage, version):
    numeric = version.split('-', 1)[0]
    shared = root / 'build' / f'libvuptsdk-base.so.{numeric}'
    dynamic = subprocess.check_output(['readelf', '-d', str(shared)], text=True)
    if '(RPATH)' in dynamic or '(RUNPATH)' in dynamic:
        raise ValueError('refusing a release library with RPATH/RUNPATH')
    files = {
        shared: f'usr/lib/{shared.name}',
        root / 'build/libvuptsdk-base.a': 'usr/lib/libvuptsdk-base.a',
        root / 'include/zuptsdk.h': 'usr/include/libvuptsdk-base/zuptsdk.h',
    }
    for source in [root / 'README.md', root / 'README.pt-BR.md', root / 'NOTICE',
                   root / 'UPSTREAM.json', *root.glob('LICENSE*')]:
        if source.is_file():
            files[source] = f'usr/share/doc/libvuptsdk-base/{source.name}'
    for source, relative in files.items():
        target = stage / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(0o644)
    subprocess.run(['strip', '--strip-unneeded', str(stage / files[shared])], check=True)
    subprocess.run(['strip', '--strip-debug', str(stage / 'usr/lib/libvuptsdk-base.a')], check=True)
    installer = (root / 'packaging/install-binary.sh').read_text().replace('@VERSION@', numeric).replace('@RELEASE@', version)
    (stage / 'install.sh').write_text(installer)
    (stage / 'install.sh').chmod(0o755)
    manifest = []
    for path in sorted(stage.rglob('*')):
        if path.is_file():
            manifest.append({'path': path.relative_to(stage).as_posix(),
                             'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                             'size': path.stat().st_size})
    (stage / 'PACKAGE-MANIFEST.json').write_text(json.dumps({'files': manifest}, indent=2) + '\n')


def compress(stage, output, zupt, threads):
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    # Zupt generates archive UUID/time metadata, so verify content, not byte
    # reproducibility. Keep its descriptor walk in private temporary storage:
    # published Zupt 5.2.9 requires readable intermediate directories.
    with tempfile.TemporaryDirectory(prefix='libvuptsdk-check-') as tmp:
        candidate = Path(tmp) / 'package.zupt'
        subprocess.run([zupt, 'compress', '--vv', '--solid', '-l', '9', '-t', str(threads),
                        str(candidate), stage.name], cwd=stage.parent, check=True)
        subprocess.run([zupt, 'test', str(candidate)], check=True)
        subprocess.run([zupt, 'extract', '-o', tmp, str(candidate)], check=True)
        recovered = Path(tmp) / stage.name
        expected = {p.relative_to(stage) for p in stage.rglob('*') if p.is_file()}
        actual = {p.relative_to(recovered) for p in recovered.rglob('*') if p.is_file()}
        if expected != actual:
            raise ValueError('release extraction file inventory mismatch')
        for rel in expected:
            if (stage / rel).read_bytes() != (recovered / rel).read_bytes():
                raise ValueError(f'release extraction mismatch: {rel}')
        # Publish only a fully verified archive, using a temporary file on the
        # destination filesystem so replacement is atomic across mounts.
        pending = None
        try:
            with tempfile.NamedTemporaryFile(prefix='.' + output.name + '.',
                                             dir=output.parent, delete=False) as stream:
                pending = Path(stream.name)
                with candidate.open('rb') as source:
                    shutil.copyfileobj(source, stream)
                stream.flush()
                os.fsync(stream.fileno())
            pending.chmod(0o644)
            pending.replace(output)
        finally:
            if pending is not None:
                pending.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=['source', 'binary'])
    parser.add_argument('--version', required=True)
    parser.add_argument('--zupt', default=os.environ.get('ZUPT', 'zupt'))
    parser.add_argument('--threads', type=int, default=4)
    args = parser.parse_args()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?', args.version):
        parser.error('invalid release version')
    if not 1 <= args.threads <= 64:
        parser.error('threads must be between 1 and 64')
    root = Path(__file__).resolve().parents[1]
    name = 'libvuptsdk-base-' + args.version
    if args.kind == 'binary':
        if platform.system() != 'Linux' or platform.machine() != 'x86_64':
            parser.error('the binary release target currently supports Linux x86_64 only')
        name += '-linux-x86_64'
    output = root / 'dist' / (name + ('-src' if args.kind == 'source' else '') + '.zupt')
    with tempfile.TemporaryDirectory(prefix='libvuptsdk-package-') as tmp:
        stage = Path(tmp) / name
        if args.kind == 'source':
            stage_source(root, stage)
        else:
            stage_binary(root, stage, args.version)
        compress(stage, output, args.zupt, args.threads)
    print(hashlib.sha256(output.read_bytes()).hexdigest(), output)


if __name__ == '__main__':
    main()
