"""Check player-vs-source archive boundaries without building PS4RT."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location('valkyria_packager_layout', TOOLS / 'package_valkyria.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PlayerReleaseLayoutTests(unittest.TestCase):
    def test_runtime_files_have_no_docs_and_keep_notices(self):
        with tempfile.TemporaryDirectory() as temp:
            output = io.StringIO()
            with patch.object(sys, 'argv', [
                'package_valkyria.py', '--runtime', '--bundle-from', temp, '--preview'
            ]), contextlib.redirect_stdout(output):
                package.main()
        preview = json.loads(output.getvalue())
        player = preview['runtime_files']
        source = preview['source_files']
        self.assertIn('docs/DISTRIBUTING_LINUX.md', source)
        self.assertIn('docs/UPSTREAM_CREDITS.md', source)
        self.assertIn('src/runtime.c', source)
        self.assertIn('LICENSE', player)
        self.assertIn('CREDITS.md', player)
        self.assertIn('licenses/GPL-3.0.txt', player)
        self.assertIn('gpu/third_party/imgui/LICENSE.txt', player)
        self.assertIn('Play-Game.sh', player)
        self.assertIn('Play-Valkyria.sh', player)  # compatibility alias
        self.assertNotIn('README.md', player)
        self.assertTrue(all(not x.startswith(('docs/', 'source/')) for x in player), player)

    def test_player_quick_start_uses_repo_url_not_release_revision(self):
        s = (TOOLS / 'package_valkyria.py').read_text()
        self.assertIn("add('PLAYER_README.txt'", s)
        self.assertIn('github.com/HekamiAeon/valkyria-revolution-ps4rt', s)
        self.assertIn('if not args.runtime:', s)
        self.assertIn("ROOT / 'packaging/linux/PLAYER_README.txt'", s)
        readme = (Path(__file__).resolve().parents[1] / 'packaging/linux/PLAYER_README.txt').read_text()
        self.assertIn('bash Play-Game.sh --game', readme)
        self.assertIn('bash Play-Game.sh', readme)
        build = (Path(__file__).resolve().parents[1] / 'packaging/linux/build-runtime.sh').read_text()
        self.assertIn('PLAYER_README.txt', build)


if __name__ == '__main__':
    unittest.main()
