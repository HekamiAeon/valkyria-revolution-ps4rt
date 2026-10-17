#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Explicit, pinned source-v07 distribution boundary; no directory globbing."""
import hashlib
import json
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parent.parent
LOCK_PATH = Path('packaging/fsr4-v07-assets.json')


def load_lock(root=ROOT):
    lock = json.loads((root / LOCK_PATH).read_text())
    if lock.get('schema_version') != 1 or len(lock['files']) != 189:
        raise ValueError('Unsupported/incomplete FSR4 v07 asset lock')
    for name in lock['files']:
        path = PurePosixPath(name)
        if len(path.parts) != 2 or path.parts[0] != 'fsr4_shaders' or '..' in path.parts:
            raise ValueError(f'Invalid locked FSR4 path: {name}')
    if lock['license_notice'] not in lock['files']:
        raise ValueError('FSR4 v07 license notice missing from lock')
    return lock


def verified_assets(root=ROOT):
    """Reject missing, substituted or symlinked assets before creating a package."""
    lock = load_lock(root)
    for name, expected in lock['files'].items():
        path = root / name
        if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(root.resolve()):
            raise ValueError(f'Missing or nonregular FSR4 v07 asset: {name}')
        data = path.read_bytes()
        if len(data) != expected['size'] or hashlib.sha256(data).hexdigest() != expected['sha256']:
            raise ValueError(f'FSR4 v07 size/checksum mismatch: {name}')
    return lock
