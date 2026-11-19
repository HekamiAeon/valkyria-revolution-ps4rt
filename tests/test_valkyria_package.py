# SPDX-License-Identifier: GPL-2.0-or-later
"""The distribution boundary must reject game/private files even if tracked."""
import importlib.util
from pathlib import Path
import re
import shutil
import subprocess
import sys
import unittest

# Import the packaging script as a module even when unittest runs from the
# repository root; direct script execution adds tools/ to sys.path automatically.
TOOLS_DIR = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS_DIR))
spec = importlib.util.spec_from_file_location('package_valkyria',
    TOOLS_DIR / 'package_valkyria.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackageBoundary(unittest.TestCase):
    def test_source_and_license_closure(self):
        for name in ('LICENSE', 'build.sh', 'scripts/prepare.py',
                     'gpu/third_party/fsr-vulkan/LICENSE.txt',
                     'gpu/third_party/fonts/LICENSE-DejaVu.txt',
                     'third_party/LibAtrac9/LICENSE',
                     'tests/data/isa_fixture.bin'):
            with self.subTest(name=name):
                self.assertTrue(package.source_allowed(name))

    def test_private_and_proprietary_inputs(self):
        for name in ('out/ps4rt-probe', 'out/valkyria/boot-linked.bin',
                     'original/CUSA06810/eboot.bin', 'docs/eboot.bin',
                     'docs/libc.prx', 'docs/sce_sys/param.sfo', 'src/texture.g1t',
                     'docs/captures/frame.png', 'src/out/guest.c',
                     'gpu/fsr4_shaders/model.bin', 'launcher/ps4rt.png',
                     'launcher/ps4rt-font.ttf', '../src/runtime.c', '/src/runtime.c',
                     'docs/savedata/slot.json', '.git/config'):
            with self.subTest(name=name):
                self.assertFalse(package.source_allowed(name))

    def test_public_docs_are_explicitly_allowlisted(self):
        for name in (*package.PUBLIC_DOCS, *package.PUBLIC_MEDIA, *package.PUBLIC_DOC_LICENSES):
            with self.subTest(allowed=name):
                self.assertTrue(package.source_allowed(name))
        for name in ('docs/notes.txt', 'docs/images/readme/private.png',
                     'docs/images/other.png', 'docs/agents/secret.md',
                     'docs/images/readme/.gitkeep',
                     'docs/licenses/unreviewed.pdf', 'docs/licenses/game.bin'):
            with self.subTest(rejected=name):
                self.assertFalse(package.source_allowed(name))

    def test_player_archive_has_no_docs_folder(self):
        self.assertFalse(any(dest.startswith('docs/') for dest in package.RUNTIME))
        self.assertFalse(any(dest.startswith('docs/') for dest in package.PLAYER_LICENSE_FILES))
        self.assertFalse(any(dest.startswith('docs/') for dest in package.OVERLAY_ASSET_NAMES))

    def test_trophy_helpers_ship_without_player_state(self):
        for name in ('scripts/trophy_catalog.py', 'scripts/trophy_crypto.py'):
            self.assertEqual(package.RUNTIME[name], name)
            self.assertTrue(package.source_allowed(name))
        for name in ('out/valkyria/user/achievements/1/NPWR11697_00.dat',
                     'scripts/achievements/catalog.tsv', 'src/trophy00.trp',
                     'scripts/npbind.dat', 'gpu/TROP.ESFM', 'gpu/TROP.SFM',
                     'references/Code-Fairy-PS4RT/scripts/trophy_catalog.py'):
            self.assertFalse(package.source_allowed(name))

    def test_binary_container_boundary(self):
        fixture_name = 'tests/data/isa_fixture.bin'
        fixture = (Path(__file__).resolve().parents[1] / fixture_name).read_bytes()
        self.assertEqual(fixture[:8], b'PS4RTI01')
        self.assertFalse(package.is_game_code_container(fixture_name, fixture))
        self.assertTrue(package.is_game_code_container('docs/boot.bin', fixture))
        self.assertTrue(package.is_game_code_container(fixture_name, fixture + b'X'))
        for magic in (b'PS4RTB05', b'PS4RTP02', b'PS4RTC01', b'PS4RTI01', b'O\x15=\x1d'):
            with self.subTest(magic=magic):
                self.assertTrue(package.is_game_code_container('src/example.bin', magic + b'123'))

    def test_runtime_copies_only_named_host_binaries(self):
        binaries = {dest: src for dest, src in package.RUNTIME.items()
                    if dest.startswith('bin/')}
        self.assertEqual(binaries, {
            'bin/ps4rt': 'bin/ps4rt',
            'bin/ps4rt-gpu-capabilities': 'bin/ps4rt-gpu-capabilities',
            'bin/gpu/libps4rtgpu.so': 'bin/gpu/libps4rtgpu.so',
        })
        for src in package.RUNTIME.values():
            self.assertNotIn('out/valkyria', src)
            self.assertNotIn('original', Path(src).parts)

    def test_source_requires_credits_and_license_but_not_the_readme(self):
        self.assertIn('LICENSE', package.REQUIRED_SOURCE)
        self.assertIn('docs/UPSTREAM_CREDITS.md', package.REQUIRED_SOURCE)
        self.assertIn('docs/licenses/GPL-3.0.txt', package.REQUIRED_SOURCE)
        self.assertNotIn('README.md', package.REQUIRED_SOURCE)


ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.run(['git', '-C', str(ROOT), *args], capture_output=True, check=True).stdout


def export_ignored(paths):
    """Whether this checkout's .gitattributes leaves each path out of `git archive`."""
    fields = git('check-attr', '-z', 'export-ignore', '--', *paths).split(b'\0')
    return {fields[i].decode(): fields[i + 2] == b'set' for i in range(0, len(fields) - 2, 3)}


def tracked(*paths):
    return [p.decode() for p in git('ls-files', '-z', '--', *paths).split(b'\0') if p]


# Project documentation: kept out of source archives, except the credits and docs/licenses/.
PROJECT_DOCS = ('README.md', 'docs', 'packaging/linux/README.md', 'packaging/github/README.md')


def project_doc(path):
    return path in PROJECT_DOCS or path.startswith('docs/')


@unittest.skipUnless(shutil.which('git') and (ROOT / '.git').exists(), 'needs a Git checkout')
class SourceArchiveDocsPolicy(unittest.TestCase):
    """Project docs stay out of GitHub's generated archives and the package_valkyria.py source
    export (both use git archive), so they can change after a release is built."""

    def test_project_docs_are_left_out_of_source_archives(self):
        docs = [p for p in tracked(*PROJECT_DOCS)
                if p != 'docs/UPSTREAM_CREDITS.md' and not p.startswith('docs/licenses/')]
        for name in ('README.md', 'docs/RUNNING_LINUX.md', 'packaging/linux/README.md',
                     'packaging/github/README.md'):
            self.assertIn(name, docs)
        for path, ignored in export_ignored(docs).items():
            with self.subTest(path=path):
                self.assertTrue(ignored)

    def test_credits_licenses_and_notices_stay_in_source_archives(self):
        notice = re.compile(r'(^|/)(license|notice|copying|readme)[^/]*$', re.IGNORECASE)
        keep = ['docs/UPSTREAM_CREDITS.md', 'docs/licenses', 'docs/licenses/GPL-3.0.txt',
                'docs/licenses/nested/NOTICE.txt', 'packaging/linux/PLAYER_README.txt',
                *(p for p in tracked() if notice.search(p) and not project_doc(p))]
        self.assertIn('LICENSE', keep)
        self.assertIn('gpu/third_party/imgui/LICENSE.txt', keep)
        for path, ignored in export_ignored(keep).items():
            with self.subTest(path=path):
                self.assertFalse(ignored)

    def test_required_source_is_never_left_out(self):
        for path, ignored in export_ignored(sorted(package.REQUIRED_SOURCE)).items():
            with self.subTest(path=path):
                self.assertFalse(ignored)


if __name__ == '__main__':
    unittest.main()
