#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Export reviewed tracked source, optionally with native Linux x86-64 binaries.

Never recursively copy a working directory or out/: those contain game code,
saves and captures. The runtime list is explicit; source comes from Git blobs.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
from fsr4_assets import load_lock, verified_assets

ROOT = Path(__file__).resolve().parent.parent
FSR4_LOCK = load_lock(ROOT)
FSR4_FILES = FSR4_LOCK['files']
SOURCE_DIRS = {'src', 'gpu', 'third_party', 'scripts', 'tools', 'tests', 'docs',
               'packaging', 'patches', 'launcher', '.github'}
SOURCE_FILES = {'LICENSE', 'README.md', 'build.sh',
                'build-headless.sh', 'run-valkyria.sh', 'run.py', 'run.sh',
                'shell.nix', 'flake.nix', 'flake.lock', '.gitignore', '.gitmodules',
                '.gitattributes', 'README.es.md', 'README.pt-BR.md', 'msys2-env.sh'}
# Inherited launcher artwork/font are unused by the Valkyria runtime; do not ship
# material with unreviewed game-art provenance in a new community package.
EXCLUDED = {'launcher/ps4rt.png', 'launcher/ps4rt.ico', 'launcher/ps4rt-font.ttf'}
# Only this reviewed synthetic binary test fixture is safe to include in source archives.
# Exact hash verification prevents a replaced game-code container being smuggled in here.
SYNTHETIC_FIXTURES = {
    'tests/data/isa_fixture.bin': 'ff9a0ebd8e4ab8b97bed867ef88ac97d813de7480ba386c719abf356e8b5c28a',
}
GAME_CONTAINER_PREFIXES = (b'PS4RTB', b'PS4RTP', b'PS4RTC', b'PS4RTI', b'O\x15=\x1d')
RUNTIME = {
    'Play-Valkyria.sh': 'packaging/linux/Play-Valkyria.sh',
    'bin/ps4rt-probe': 'out/ps4rt-probe',
    'bin/gpu/libps4rtgpu.so': 'out/gpu/libps4rtgpu.so',
    'scripts/valkyria_launch.py': 'scripts/valkyria_launch.py',
    'scripts/prepare.py': 'scripts/prepare.py',
    'scripts/link_modules.py': 'scripts/link_modules.py',
    'scripts/link_libc.py': 'scripts/link_libc.py',
    'scripts/content_profile.py': 'scripts/content_profile.py',
    'tools/fetch_fsr4_assets.sh': 'tools/fetch_fsr4_assets.sh',
    'tools/fsr4_optimize.sh': 'tools/fsr4_optimize.sh',
    'tools/fsr4_post_lds.pl': 'tools/fsr4_post_lds.pl',
    'tools/fsr4_pass11_guard.pl': 'tools/fsr4_pass11_guard.pl',
    'tools/Fsr4SpirvCrossFixes.pm': 'tools/Fsr4SpirvCrossFixes.pm',
    'LICENSE': 'LICENSE',
    'licenses/GPL-3.0.txt': 'docs/licenses/GPL-3.0.txt',
    'CREDITS.md': 'docs/UPSTREAM_CREDITS.md',
    'README.md': 'docs/DISTRIBUTING_LINUX.md',
}


def source_allowed(name):
    path = PurePosixPath(name)
    # Keep internal agent instructions, development diaries and captures out of releases.
    if path.name in {'AGENTS.md', 'CLAUDE.md', 'PORTING_LOG.md'}:
        return False
    if any(name == prefix or name.startswith(prefix + '/') for prefix in
           ('docs/agents', 'docs/plans', 'docs/specs', 'out', 'original', 'references')):
        return False
    if name.startswith(('PUBLIC_EXPORT_MANIFEST', 'docs/upstream/SOURCE_EXPORT_MANIFEST')):
        return False
    if not path.parts or path.is_absolute() or '..' in path.parts or name in EXCLUDED:
        return False
    # The only allowed external shader/model assets are listed in the pinned lock.
    if path.parts[0] == 'fsr4_shaders':
        return name in FSR4_FILES
    if any(p in {'out', 'original', 'references', 'savedata', 'captures',
                 'frame-dump', 'fsr4_shaders', 'fsr4_411', '__pycache__', '.git'} for p in path.parts):
        return False
    if path.suffix.lower() in {'.pkg', '.prx', '.self', '.elf', '.sfo', '.cpk', '.usm',
                              '.g1t', '.g1m', '.at9'}:
        return False
    if path.suffix.lower() == '.bin' and name != 'tests/data/isa_fixture.bin':
        return False
    return name in SOURCE_FILES or path.parts[0] in SOURCE_DIRS


def is_game_code_container(name, data):
    if not data.startswith(GAME_CONTAINER_PREFIXES):
        return False
    expected = SYNTHETIC_FIXTURES.get(name)
    return not (expected and hashlib.sha256(data).hexdigest() == expected)


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', action='store_true', help='Include Linux x86-64 binaries (tested on Fedora 44; no system libraries)')
    parser.add_argument('--preview', action='store_true', help='Print the exact file list without creating an archive')
    parser.add_argument('--output', type=Path, default=ROOT / 'out/community')
    args = parser.parse_args()
    revision = git('rev-parse', 'HEAD').decode().strip()
    files = git('ls-tree', '-r', '--name-only', 'HEAD').decode().splitlines()
    source_files = [f for f in files if source_allowed(f)]
    runtime_files = dict(RUNTIME)
    runtime_files.update({name: name for name in FSR4_FILES})
    if args.preview:
        print(json.dumps({'revision': revision, 'source_files': source_files,
                          'runtime_files': runtime_files if args.runtime else {}}, indent=2))
        return
    print(f'Packaging {revision[:7]}: {len(source_files)} source files'
          + (f', {len(runtime_files)} runtime assets' if args.runtime else ''))
    if git('status', '--porcelain').strip():
        raise SystemExit('Checkpoint the working tree first; releases must identify the exact committed source.')
    if not source_files or 'build.sh' not in source_files:
        raise SystemExit('Source export is incomplete')
    verified_assets(ROOT)
    if not set(FSR4_FILES).issubset(source_files):
        raise SystemExit('Commit all locked FSR4 v07 assets before exporting')
    args.output.mkdir(parents=True, exist_ok=True)
    kind = 'linux-x86_64' if args.runtime else 'source'
    stem = f'valkyria-revolution-{kind}-{revision[:7]}'
    target = args.output / (stem + '.tar.gz')
    if target.exists():
        raise SystemExit(f'Archive already exists: {target}')
    manifest = {'revision': revision, 'kind': kind, 'files': {}}
    if args.runtime:
        manifest.update(label='Linux x86-64 — currently tested on Fedora 44',
                        tested_on=['Fedora 44'], fsr4_v07={
                            'asset_commit': FSR4_LOCK['asset_commit'],
                            'license_notice': FSR4_LOCK['license_notice'],
                            'output_tiers': FSR4_LOCK['output_tiers'],
                            'asset_files': len(FSR4_FILES)})
    staging = target.with_suffix(target.suffix + '.part')
    # An interrupted attempt can leave this temporary archive behind.
    if staging.is_symlink():
        raise SystemExit(f'Refusing symlink staging path: {staging}')
    staging.unlink(missing_ok=True)
    with tarfile.open(staging, 'x:gz') as archive:
        def add(name, data, mode=0o644):
            info = tarfile.TarInfo(stem + '/' + name)
            info.size, info.mode = len(data), mode
            archive.addfile(info, io.BytesIO(data))
            manifest['files'][name] = hashlib.sha256(data).hexdigest()
        # Reading Git's archive includes vendored third-party sources and notices,
        # without untracked files, sub-repository metadata, or local modifications.
        with tarfile.open(fileobj=io.BytesIO(git('archive', '--format=tar', 'HEAD'))) as source:
            for member in source:
                if member.name not in source_files:
                    continue
                if not member.isfile():
                    raise ValueError(f'Unreviewed source symlink/submodule: {member.name}')
                data = source.extractfile(member).read()
                if is_game_code_container(member.name, data):
                    raise ValueError(f'Game-code container in source: {member.name}')
                add(('source/' if args.runtime else '') + member.name, data, member.mode)
        if args.runtime:
            for dest, src in runtime_files.items():
                path = ROOT / src
                if path.is_symlink() or not path.is_file():
                    raise ValueError(f'Expected regular file: {src}')
                data = path.read_bytes()
                if dest.startswith('bin/') and not data.startswith(b'\x7fELF'):
                    raise ValueError(f'Expected native host ELF: {src}')
                add(dest, data, 0o755 if dest.startswith('bin/') or dest.endswith('.sh') else 0o644)
        add('MANIFEST.json', (json.dumps(manifest, indent=2, sort_keys=True) + '\n').encode())
    staging.rename(target)
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_suffix(target.suffix + '.sha256').write_text(f'{digest}  {target.name}\n')
    print(f'Created {target} ({target.stat().st_size:,} bytes)')


if __name__ == '__main__':
    main()
