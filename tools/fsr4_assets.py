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
    files = lock.get('files')
    tiers = lock.get('output_tiers')

    if lock.get('schema_version') != 1 or not isinstance(files, dict):
        raise ValueError('Unsupported FSR4 v07 asset lock')
    if tiers != ['1080', '2160', '4320']:
        raise ValueError('Unexpected FSR4 output tiers')

    required = {
        'fsr4_shaders/LICENSE-FSR4-v07.txt',
        'fsr4_shaders/rcas.spv',
        'fsr4_shaders/spd_auto_exposure.spv',
    }

    presets = (
        'native', 'quality', 'balanced',
        'performance', 'ultraperf', 'drs'
    )
    stages = ('pre', 'post') + tuple(
        f'pass{i}' for i in range(1, 13)
    )

    for preset in presets:
        stem = f'fsr4_shaders/fsr4_model_v07_i8_{preset}'
        required.update((
            stem + '_shader_manifest.json',
            stem + '_initializers.bin',
            stem + '_pre_weights.bin',
        ))
        for tier in tiers:
            required.update(
                f'{stem}_{tier}_{stage}.spv'
                for stage in stages
            )

    if set(files) != required:
        raise ValueError(
            f'FSR4 inventory mismatch: '
            f'missing={len(required - set(files))}, '
            f'unexpected={len(set(files) - required)}'
        )

    if lock.get('license_notice') != 'fsr4_shaders/LICENSE-FSR4-v07.txt':
        raise ValueError('FSR4 license notice mismatch')

    for name, metadata in files.items():
        path = PurePosixPath(name)
        if len(path.parts) != 2 or path.parts[0] != 'fsr4_shaders':
            raise ValueError(f'Invalid FSR4 path: {name}')
        if not isinstance(metadata, dict):
            raise ValueError(f'Invalid FSR4 metadata: {name}')
        size = metadata.get('size')
        sha = metadata.get('sha256')
        if (type(size) is not int or size < 0 or
                not isinstance(sha, str) or len(sha) != 64 or
                any(c not in '0123456789abcdef' for c in sha)):
            raise ValueError(f'Invalid FSR4 checksum metadata: {name}')

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
