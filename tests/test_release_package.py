#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Cristian Cezar Moisés
"""Exercise real Zupt source export, extraction and manifest validation."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReleasePackageTest(unittest.TestCase):
    def test_source_roundtrip_and_extracted_reexport(self):
        spec = importlib.util.spec_from_file_location(
            'release_package', ROOT / 'packaging' / 'build_zupt.py')
        package = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(package)
        with tempfile.TemporaryDirectory(prefix='sdk-release-test-') as tmp:
            root = Path(tmp) / 'repo'
            root.mkdir()
            subprocess.run(['git', 'init', '-q', root], check=True)
            (root / 'README.md').write_text('Release fixture: ação\n')
            (root / 'run.sh').write_text('#!/bin/sh\nexit 0\n')
            (root / 'run.sh').chmod(0o755)
            (root / 'prebuilt').mkdir()
            (root / 'prebuilt' / 'old.so').write_bytes(b'excluded binary')
            subprocess.run(['git', '-C', root, 'add', '.'], check=True)
            (root / 'untracked-secret').write_text('must not enter the package')
            stage = Path(tmp) / 'stage'
            package.stage_source(root, stage)
            self.assertFalse((stage / 'prebuilt').exists())
            self.assertFalse((stage / 'untracked-secret').exists())
            manifest = json.loads((stage / 'SOURCE-MANIFEST.json').read_text())
            self.assertEqual({f['path'] for f in manifest['files']}, {'README.md', 'run.sh'})
            for item in manifest['files']:
                self.assertEqual(hashlib.sha256((stage / item['path']).read_bytes()).hexdigest(), item['sha256'])
            archive = Path(tmp) / 'fixture.zupt'
            package.compress(stage, archive, 'zupt', 2)
            extracted = Path(tmp) / 'extracted'
            subprocess.run(['zupt', 'extract', '-o', extracted, archive], check=True,
                           stdout=subprocess.DEVNULL)
            recovered = extracted / stage.name
            self.assertEqual((recovered / 'README.md').read_bytes(), (root / 'README.md').read_bytes())
            exported = Path(tmp) / 'reexported'
            package.stage_source(recovered, exported)
            self.assertEqual((exported / 'run.sh').read_bytes(), (root / 'run.sh').read_bytes())
            (recovered / 'run.sh').write_text('tampered')
            with self.assertRaisesRegex(ValueError, 'manifest'):
                package.stage_source(recovered, Path(tmp) / 'tampered')

    def test_manifest_cannot_escape_source_root(self):
        spec = importlib.util.spec_from_file_location(
            'release_package', ROOT / 'packaging' / 'build_zupt.py')
        package = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(package)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'SOURCE-MANIFEST.json').write_text(json.dumps({
                'files': [{'path': '../outside', 'sha256': '0' * 64, 'mode': 420}]}))
            with self.assertRaisesRegex(ValueError, 'path'):
                package.stage_source(root, root / 'out')

    def test_publish_through_execute_only_ancestor(self):
        spec = importlib.util.spec_from_file_location(
            'release_package', ROOT / 'packaging' / 'build_zupt.py')
        package = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(package)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            stage = root / 'source'
            stage.mkdir()
            (stage / 'data').write_bytes(b'archive payload')
            ancestor = root / 'traversable'
            output_dir = ancestor / 'output'
            output_dir.mkdir(parents=True)
            ancestor.chmod(0o111)
            try:
                output = output_dir / 'release.zupt'
                package.compress(stage, output, 'zupt', 1)
                self.assertTrue(output.is_file())
            finally:
                ancestor.chmod(0o700)


if __name__ == '__main__':
    unittest.main()
