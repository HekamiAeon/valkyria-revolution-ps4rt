"""Regression coverage for Valkyria's shared player launcher contract."""
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    'valkyria_player_launcher', ROOT / 'scripts/valkyria_launch.py')
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class PlayerLauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.bundle = self.base / 'bundle'
        (self.bundle / 'bin').mkdir(parents=True)
        (self.bundle / 'bin/ps4rt').write_bytes(b'pretend native binary')
        self.data = self.base / 'data'
        self.game_a = self.make_game('first')
        self.game_b = self.make_game('second')
        self.prepare = types.ModuleType('prepare')
        self.prepare.sfo = lambda raw: {'TITLE_ID': raw.decode('ascii')}

    def make_game(self, name, title='CUSA06810'):
        game = self.base / name / 'CUSA06810'
        (game / 'sce_sys').mkdir(parents=True)
        (game / 'sce_sys/param.sfo').write_bytes(title.encode('ascii'))
        return game

    def invoke(self, *args):
        calls = []
        with patch.object(launcher, 'ROOT', self.bundle), \
             patch.dict(sys.modules, {'prepare': self.prepare}), \
             patch.dict(os.environ, {'VALKYRIA_GAME_DIR': ''}), \
             patch.object(launcher.subprocess, 'run') as runner, \
             patch.object(launcher.os, 'execve',
                          side_effect=lambda *a: calls.append(a)):
            launcher.main(['--data-dir', str(self.data), *args])
        return calls, runner

    def remembered(self):
        return (self.data / 'game-folder.txt').read_text().strip()

    def test_missing_path_explains_first_launch_without_picker(self):
        with self.assertRaisesRegex(ValueError, 'Play-Game.sh --game'):
            self.invoke()
        self.assertFalse((self.data / 'game-folder.txt').exists())
        source = (ROOT / 'scripts/valkyria_launch.py').read_text()
        self.assertNotIn('tkinter', source)
        self.assertNotIn('choose_game', source)

    def test_explicit_first_launch_then_implicit_second_launch(self):
        first, runner = self.invoke('--game', str(self.game_a))
        self.assertEqual(len(first), 1)
        self.assertEqual(runner.call_count, 3)
        self.assertEqual(self.remembered(), str(self.game_a.resolve()))
        second, runner = self.invoke()
        self.assertEqual(len(second), 1)
        self.assertEqual(runner.call_count, 3)
        self.assertEqual(self.remembered(), str(self.game_a.resolve()))
        self.assertIn(str(self.game_a.resolve()), second[0][1])

    def test_explicit_override_updates_remembered_folder(self):
        self.invoke('--game', str(self.game_a))
        calls, _ = self.invoke('--game', str(self.game_b))
        self.assertEqual(self.remembered(), str(self.game_b.resolve()))
        self.assertIn(str(self.game_b.resolve()), calls[0][1])

    def test_invalid_explicit_override_preserves_saved_folder(self):
        self.invoke('--game', str(self.game_a))
        with self.assertRaisesRegex(ValueError, 'Game folder not found'):
            self.invoke('--game', str(self.base / 'missing'))
        self.assertEqual(self.remembered(), str(self.game_a.resolve()))

    def test_stale_remembered_directory_reports_error(self):
        self.data.mkdir(parents=True)
        (self.data / 'game-folder.txt').write_text(str(self.base / 'gone') + '\n')
        with self.assertRaisesRegex(ValueError, 'Supply --game'):
            self.invoke()
        self.assertEqual(self.remembered(), str(self.base / 'gone'))

    def test_wrong_title_preserves_remembered_path(self):
        self.invoke('--game', str(self.game_a))
        bad = self.make_game('bad', title='CUSA99999')
        with self.assertRaisesRegex(ValueError, 'requires CUSA06810'):
            self.invoke('--game', str(bad))
        self.assertEqual(self.remembered(), str(self.game_a.resolve()))

    def test_prepare_only_does_not_exec_game(self):
        calls, runner = self.invoke('--prepare-only', '--game', str(self.game_a))
        self.assertFalse(calls)
        self.assertEqual(runner.call_count, 3)
        self.assertEqual(self.remembered(), str(self.game_a.resolve()))


if __name__ == '__main__':
    unittest.main()
