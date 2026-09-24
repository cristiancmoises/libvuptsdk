#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 Cristian Cezar Moisés
"""Check current source license identifiers, including adapted-code notices."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
extensions = {'.c', '.h', '.hpp', '.cpp', '.py', '.sh', '.yml', '.yaml',
              '.jazz', '.s', '.map', '.rs', '.go', '.js', '.bend'}
adapted = {'src/vv_xxh64.c': 'Apache-2.0 AND BSD-2-Clause',
           'src/zupt_xxh.c': 'Apache-2.0 AND BSD-2-Clause',
           'src/zupt_mlkem.c': 'Apache-2.0 AND CC0-1.0',
           'src/zupt_x25519.c': 'Apache-2.0 AND BSD-3-Clause'}
errors, checked = [], 0
for path in sorted(root.rglob('*')):
    rel = path.relative_to(root)
    if set(rel.parts) & {'.git', 'build', 'dist', 'release', 'prebuilt', 'target', '__pycache__'}:
        continue
    if not path.is_file() or not (path.suffix in extensions or path.name == 'Makefile'):
        continue
    expected = adapted.get(rel.as_posix(), 'Apache-2.0')
    text = path.read_text()
    matches = re.findall(r'SPDX-License-Identifier:\s*([^\n]+)', text)
    matches = [m.strip().removesuffix('*/').strip() for m in matches]
    if expected not in matches:
        errors.append(f'{rel}: expected SPDX-License-Identifier: {expected}')
    checked += 1
for error in errors:
    print(error, file=sys.stderr)
print(f'License identifiers: {checked} source files, {len(errors)} errors')
sys.exit(bool(errors))
