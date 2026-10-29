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
import os
import re
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
from fsr4_assets import load_lock, verified_assets

ROOT = Path(__file__).resolve().parent.parent
FSR4_LOCK = load_lock(ROOT)
FSR4_FILES = FSR4_LOCK['files']
SOURCE_DIRS = {'src', 'gpu', 'third_party', 'scripts', 'tools', 'tests', 'docs',
               'packaging', 'patches', 'launcher', '.github'}
PUBLIC_DOCS = {
    'docs/RUNNING_LINUX.md', 'docs/DISTRIBUTING_LINUX.md',
    'docs/UPSTREAM_CREDITS.md', 'docs/LINUX_BINARY_RELEASE.md',
}
# The public README uses this single reviewed gameplay screenshot.
PUBLIC_MEDIA = {'docs/images/readme/gameplay.png'}
PUBLIC_DOC_LICENSES = {'docs/licenses/GPL-3.0.txt'}
SOURCE_FILES = {'LICENSE', 'README.md', 'build.sh',
                'build-headless.sh', 'run-valkyria.sh', 'run.py', 'run.sh',
                'shell.nix', 'flake.nix', 'flake.lock', '.gitignore', '.gitmodules',
                '.gitattributes', 'README.es.md', 'README.pt-BR.md'}
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
    'bin/ps4rt': 'bin/ps4rt',
    'bin/ps4rt-gpu-capabilities': 'bin/ps4rt-gpu-capabilities',
    'bin/gpu/libps4rtgpu.so': 'bin/gpu/libps4rtgpu.so',
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
}


# A separate complete source archive carries docs and all developer files.
# The player archive keeps directly relevant notices without a docs/ tree.
PLAYER_LICENSE_FILES = (
    'gpu/shadps4/LICENSE',
    'gpu/third_party/fonts/LICENSE-DejaVu.txt',
    'gpu/third_party/fsr-vulkan/LICENSE.txt',
    'gpu/third_party/fsr-vulkan/NOTICE.md',
    'gpu/third_party/half/LICENSE.txt',
    'gpu/third_party/imgui/LICENSE.txt',
    'gpu/third_party/magic_enum/LICENSE',
    'gpu/third_party/miniz/LICENSE',
    'gpu/third_party/sirit/LICENSE.txt',
    'gpu/third_party/xbyak/COPYRIGHT',
    'third_party/LibAtrac9/LICENSE',
)

LINUX_LIBRARIES = (
    'libavcodec.so.62', 'libavformat.so.62', 'libavutil.so.60',
    'libfmt.so.11', 'libgcc_s.so.1', 'libSDL3.so.0',
    'libstdc++.so.6', 'libswresample.so.6', 'libswscale.so.9',
    'libxxhash.so.0', 'libz.so.1', 'libZydis.so.5.0',
)
EXPECTED_RUNPATH = {
    'bin/ps4rt': '$ORIGIN/../lib:$ORIGIN/gpu',
    'bin/gpu/libps4rtgpu.so': '$ORIGIN/../../lib',
    'bin/ps4rt-gpu-capabilities': '$ORIGIN/../lib',
    **{'lib/' + name: '$ORIGIN' for name in LINUX_LIBRARIES},
}


def verify_bundle(bundle):
    """Validate the exact tested, relocatable Linux inputs before packaging."""
    if not bundle.is_dir():
        raise SystemExit(f'Linux bundle not found: {bundle}')
    for rel, expected in EXPECTED_RUNPATH.items():
        path = bundle / rel
        if path.is_symlink() or not path.is_file():
            raise SystemExit('Missing or symlinked ELF in bundle: ' + rel)
        actual = subprocess.check_output(
            ['patchelf', '--print-rpath', str(path)], text=True).strip()
        if actual != expected:
            raise SystemExit(f'Unexpected RUNPATH for {rel}: {actual!r}')
        info = subprocess.check_output(
            ['readelf', '-W', '--version-info', str(path)], text=True)
        versions = [
            tuple(map(int, match.split('.')))
            for match in re.findall(r'GLIBC_(\d+(?:\.\d+)+)', info)
        ]
        if versions and max(versions) > (2, 35):
            raise SystemExit(f'glibc newer than 2.35: {rel}')
    env = os.environ.copy()
    env.pop('LD_LIBRARY_PATH', None)
    env.pop('LD_PRELOAD', None)
    for rel in ('bin/ps4rt', 'bin/gpu/libps4rtgpu.so', 'bin/ps4rt-gpu-capabilities'):
        result = subprocess.run(['ldd', str(bundle / rel)], capture_output=True, text=True, env=env)
        if result.returncode or 'not found' in result.stdout + result.stderr:
            raise SystemExit('Host dependency failure for ' + rel + '\n' + result.stdout + result.stderr)
        for line in result.stdout.splitlines():
            m = re.match(r'\s*(\S+)\s+=>\s+(/\S+)\s+\(', line)
            if m and (bundle / 'lib' / m.group(1)).is_file():
                if Path(m.group(2)).resolve() != (bundle / 'lib' / m.group(1)).resolve():
                    raise SystemExit('Bundled dependency escaped bundle: ' + line)


OVERLAY_ASSET_NAMES = (
    'gpu/assets/valkyria_overlay.png',
    'gpu/assets/sounds/navigate.wav',
    'gpu/assets/sounds/confirm.wav',
    'gpu/assets/sounds/cancel.wav',
    'gpu/assets/sounds/open.wav',  # optional; synthetic fallback if absent
)


def validate_overlay_asset(name, payload):
    if len(payload) > 8 * 1024 * 1024:
        raise SystemExit('Oversized overlay asset: ' + name)
    if name.endswith('.png'):
        if len(payload) < 24 or not payload.startswith(b'\x89PNG\r\n\x1a\n'):
            raise SystemExit('Invalid menu portrait PNG: ' + name)
        import struct
        w, h = struct.unpack('>II', payload[16:24])
        if w == 0 or h == 0 or w > 4096 or h > 4096:
            raise SystemExit('Portrait dimensions outside 4096x4096: ' + name)
    elif len(payload) < 12 or payload[:4] != b'RIFF' or payload[8:12] != b'WAVE' or len(payload) > 2 * 1024 * 1024:
        raise SystemExit('Invalid menu audio WAV: ' + name)


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
    # Match Sakura's public-doc allowlist; never archive an arbitrary docs/ tree.
    if name.startswith('docs/') and name not in (
            PUBLIC_DOCS | PUBLIC_MEDIA | PUBLIC_DOC_LICENSES):
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
    parser.add_argument('--runtime', action='store_true', help='Include audited glibc 2.35 Linux binaries and libraries')
    parser.add_argument('--bundle-from', type=Path,
                        help='Verified Ubuntu-built ELF and lib/ bundle (required for --runtime)')
    parser.add_argument('--preview', action='store_true', help='Print the exact file list without creating an archive')
    parser.add_argument('--output', type=Path, default=ROOT / 'out/community')
    args = parser.parse_args()
    if args.runtime and args.bundle_from is None:
        parser.error('--runtime requires --bundle-from PATH')
    bundle = args.bundle_from.expanduser().resolve() if args.bundle_from else None
    revision = git('rev-parse', 'HEAD').decode().strip()
    files = git('ls-tree', '-r', '--name-only', 'HEAD').decode().splitlines()
    source_files = [f for f in files if source_allowed(f)]
    runtime_files = dict(RUNTIME)
    runtime_files.update({name: name for name in PLAYER_LICENSE_FILES})
    if args.runtime:
        runtime_files.update({'lib/' + name: 'lib/' + name for name in LINUX_LIBRARIES})
        # Copy precisely the committed media staged and tested with this build.
        for name in OVERLAY_ASSET_NAMES:
            if (bundle / name).is_file():
                runtime_files[name] = name
    runtime_files.update({name: name for name in FSR4_FILES})
    if args.preview:
        print(json.dumps({'revision': revision, 'source_files': source_files,
                          'runtime_files': runtime_files if args.runtime else {}}, indent=2))
        return
    print(f'Packaging {revision[:7]}: {len(source_files)} source files'
          + (f', {len(runtime_files)} runtime assets' if args.runtime else ''))
    if args.runtime:
        verify_bundle(bundle)
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
        manifest.update(label='Linux x86-64, glibc 2.35+, Fedora gameplay tested',
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
                # Player archives have no embedded source tree or docs/. The
                # separately published source archive retains these files.
                if not args.runtime:
                    add(member.name, data, member.mode)
        if args.runtime:
            for dest, src in runtime_files.items():
                path = (bundle if dest.startswith(('bin/', 'lib/', 'gpu/assets/')) else ROOT) / src
                if path.is_symlink() or not path.is_file():
                    raise ValueError(f'Expected regular file: {src}')
                data = path.read_bytes()
                if dest.startswith(('bin/', 'lib/')) and not data.startswith(b'\x7fELF'):
                    raise ValueError(f'Expected native host ELF: {src}')
                if dest.startswith('gpu/assets/'):
                    validate_overlay_asset(dest, data)
                add(dest, data, 0o755 if dest.startswith('bin/') or dest.endswith('.sh') else 0o644)
        if args.runtime:
            add('PLAYER_README.txt', (
                'Valkyria Revolution - PS4RT Linux x86-64\n'
                'Run: bash Play-Valkyria.sh --game "/path/to/CUSA06810"\n'
                'Instructions: https://github.com/HekamiAeon/valkyria-revolution-ps4rt\n'
                'Requires your own plaintext extracted PS4 game. No game files included.\n'
            ).encode())
        add('MANIFEST.json', (json.dumps(manifest, indent=2, sort_keys=True) + '\n').encode())
    staging.rename(target)
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    target.with_suffix(target.suffix + '.sha256').write_text(f'{digest}  {target.name}\n')
    print(f'Created {target} ({target.stat().st_size:,} bytes)')


if __name__ == '__main__':
    main()
