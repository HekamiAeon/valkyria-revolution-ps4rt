# SPDX-License-Identifier: GPL-2.0-or-later
"""The distribution boundary must reject game/private files even if tracked."""
import importlib.util
from pathlib import Path
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
                    if src.startswith('out/')}
        self.assertEqual(binaries, {'bin/ps4rt-probe': 'out/ps4rt-probe',
            'bin/gpu/libps4rtgpu.so': 'out/gpu/libps4rtgpu.so'})
        for src in package.RUNTIME.values():
            self.assertNotIn('out/valkyria', src)
            self.assertNotIn('original', Path(src).parts)


if __name__ == '__main__':
    unittest.main()
