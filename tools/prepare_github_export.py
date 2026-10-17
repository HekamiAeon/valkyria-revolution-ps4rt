#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepare an isolated public source tree from an audited source archive; never push."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile
from fsr4_assets import load_lock

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'out/github/valkyria-revolution')
    args = parser.parse_args()
    archive_hash = hashlib.sha256(args.archive.read_bytes()).hexdigest()
    expected = args.archive.with_suffix(args.archive.suffix + '.sha256').read_text().split()[0]
    if archive_hash != expected:
        raise ValueError('Source archive checksum mismatch')
    destination = args.output.resolve()
    if destination.exists():
        raise ValueError(f'Refusing to overwrite existing export: {destination}')
    fsr4 = load_lock(ROOT)
    with tarfile.open(args.archive) as archive:
        members = archive.getmembers()
        if not members or not all(m.isfile() for m in members):
            raise ValueError('Expected regular source files only')
        prefix = members[0].name.split('/')[0] + '/'
        manifest = json.load(archive.extractfile(prefix + 'MANIFEST.json'))
        if manifest['kind'] != 'source':
            raise ValueError('Use the source archive, not the runtime archive')
        if len(members) != len(manifest['files']) + 1 or len({m.name for m in members}) != len(members):
            raise ValueError('Source manifest member count mismatch')
        payload = {}
        for member in members:
            if not member.name.startswith(prefix):
                raise ValueError('Unexpected source prefix')
            name = member.name[len(prefix):]
            path = PurePosixPath(name)
            forbidden = {'..', '.git', 'out', 'original', 'references', 'savedata',
                         'captures', 'frame-dump', 'fsr4_411', '.agents', '.codex'}
            if path.is_absolute() or any(p in forbidden for p in path.parts):
                raise ValueError(f'Private or external path: {name}')
            data = archive.extractfile(member).read()
            if name != 'MANIFEST.json' and hashlib.sha256(data).hexdigest() != manifest['files'][name]:
                raise ValueError(f'Payload hash mismatch: {name}')
            if 'fsr4_shaders' in path.parts:
                locked = fsr4['files'].get(name)
                if not locked or len(data) != locked['size'] or hashlib.sha256(data).hexdigest() != locked['sha256']:
                    raise ValueError(f'Unverified FSR4 v07 asset: {name}')
            if data.startswith((b'PS4RTB', b'PS4RTP', b'PS4RTC', b'PS4RTI', b'O\x15=\x1d', b'\x7fELF', b'\x7fCNT')):
                raise ValueError(f'Executable/game container in source: {name}')
            if name in {'launcher/ps4rt.png', 'launcher/ps4rt.ico', 'launcher/ps4rt-font.ttf'}:
                raise ValueError(f'Unreviewed artwork: {name}')
            # Preserve build metadata separately from project documentation.
            rename = {'.gitmodules': 'docs/source-metadata/GITMODULES',
                      'MANIFEST.json': 'docs/source-metadata/SOURCE_EXPORT_MANIFEST.json'}
            public_name = rename.get(name, name)
            if name.startswith('.github/'):
                public_name = 'docs/source-metadata/github/' + name[len('.github/'):]
            payload[public_name] = (data, member.mode)
    if not set(fsr4['files']).issubset(payload):
        raise ValueError('Source export does not contain all 189 verified FSR4 v07 assets')
    for name, source in {
        'README.md': ROOT / 'packaging/github/README.md',
        'packaging/github/README.md': ROOT / 'packaging/github/README.md',
        'tools/prepare_github_export.py': Path(__file__),
    }.items():
        payload[name] = (source.read_bytes(), 0o644)
    # Preserve the latest audit record without bringing working-tree/Git metadata.
    ignore = payload['.gitignore'][0].decode().replace('!launcher/ps4rt.png\n', '')
    ignore += '\n# Never commit locally supplied game files or reference checkouts.\n'
    ignore += '/original/\n/references/\n*.pkg\n*.prx\n*.self\n*.elf\n*.sfo\n*.cpk\n*.usm\n*.g1t\n*.g1m\n*.at9\n'
    payload['.gitignore'] = (ignore.encode(), 0o644)
    files = {}
    for name, (data, mode) in payload.items():
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        path.chmod(mode & 0o777)
        files[name] = hashlib.sha256(data).hexdigest()
    report = {'source_revision': manifest['revision'], 'source_archive_sha256': archive_hash,
              'files': files, 'active_release_workflows': [],
              'external_reference_checkouts_included': False, 'rea_tooling_included': False}
    (destination / 'PUBLIC_EXPORT_MANIFEST.json').write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    print(f'Prepared {destination}: {len(files)} payload files; no Git initialization or publication')


if __name__ == '__main__':
    main()
