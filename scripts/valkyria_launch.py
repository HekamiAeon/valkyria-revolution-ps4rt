#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Launch the port using game files supplied locally by the player."""
import argparse
import os
import shlex
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent


def choose_game(previous):
    try:
        import tkinter as tk
        from tkinter import filedialog
        window = tk.Tk()
        window.withdraw()
        selected = filedialog.askdirectory(
            title='Select your extracted Valkyria Revolution folder (CUSA06810)',
            initialdir=previous if previous else None, parent=window)
        window.destroy()
        return selected
    except Exception as error:
        raise ValueError('Pass --game /path/to/CUSA06810 (folder chooser unavailable)') from error


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', help='Your extracted, plaintext CUSA06810 game folder')
    parser.add_argument('--data-dir', type=Path, help='Private settings, saves and prepared code directory')
    parser.add_argument('--prepare-only', action='store_true', help='Prepare locally without opening the game')
    args, runtime_args = parser.parse_known_args(argv)
    if runtime_args and runtime_args[0] == '--':
        runtime_args.pop(0)
    packaged = (ROOT / 'bin/ps4rt').is_file()
    binary = ROOT / ('bin/ps4rt' if packaged else 'out/ps4rt')
    default_data = (Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) /
                    'valkyria-revolution') if packaged else ROOT / 'out/valkyria'
    data = (args.data_dir or Path(os.environ.get('VALKYRIA_DATA_DIR', default_data))).expanduser().resolve()
    remembered = data / 'game-folder.txt'
    previous = remembered.read_text().strip() if remembered.is_file() else ''
    selected = args.game or os.environ.get('VALKYRIA_GAME_DIR') or previous
    if not selected and not packaged and (ROOT.parent / 'original/CUSA06810').is_dir():
        selected = str(ROOT.parent / 'original/CUSA06810')
    selected = selected or choose_game(previous)
    if not selected:
        return 0
    game = Path(selected).expanduser().resolve()
    from prepare import sfo
    metadata = sfo((game / 'sce_sys/param.sfo').read_bytes())
    if metadata.get('TITLE_ID') != 'CUSA06810':
        raise ValueError('This port requires CUSA06810; select that extracted game folder')
    if not binary.is_file():
        raise ValueError('Runtime missing. Build with PS4RT_LTO=OFF bash build.sh --test')
    data.mkdir(parents=True, exist_ok=True)
    for directory in ('user/gpu', 'frame-dump'):
        (data / directory).mkdir(parents=True, exist_ok=True)
    for script in ('prepare.py', 'link_modules.py', 'content_profile.py'):
        subprocess.run([sys.executable, str(ROOT / 'scripts' / script), str(game),
                        '--out', str(data)], check=True)
    remembered.write_text(str(game) + '\n')
    if args.prepare_only:
        print(f'Prepared locally in {data}; original files were read only.')
        return 0
    env = os.environ.copy()
    # Always restart the same extracted game and writable data directory.
    # Preserve optional runtime arguments without depending on the shell cwd.
    restart = [sys.executable, str(Path(__file__).resolve()),
               '--game', str(game), '--data-dir', str(data)]
    if runtime_args:
        restart += ['--', *runtime_args]
    env['PS4RT_RESTART_COMMAND'] = shlex.join(restart)
    env.update(PS4RT_GPU_USER_DIR=str(data / 'user/gpu'), PS4RT_CONFIG=str(data / 'valkyria.ini'),
               PS4RT_CAPTURE_TRIGGER=str(data / 'next-render.request'),
               PS4RT_CAPTURE_DIR=str(data / 'captures'),
               PS4RT_FRAME_DUMP_TRIGGER=str(data / 'next-frame.request'),
               PS4RT_DUMP_DIR=str(data / 'frame-dump'),
               PS4RT_DUMP_TRIGGER=str(data / 'next-temporal.request'))
    env.setdefault('PS4RT_DUMP_FRAMES', '2')
    env.setdefault('PS4RT_FSR4_DIR', str(ROOT / 'fsr4_shaders'))
    # Optional portrait and menu sounds live under the runtime root.
    # The local asset folders can be populated without changing Python/C++ code.
    if not env.get('PS4RT_OVERLAY_BACKGROUND'):
        art = ROOT / 'gpu/assets/valkyria_overlay.png'
        if art.is_file():
            env['PS4RT_OVERLAY_BACKGROUND'] = str(art)
    if not env.get('PS4RT_MENU_SOUND_DIR'):
        sounds = ROOT / 'gpu/assets/sounds'
        if sounds.is_dir():
            env['PS4RT_MENU_SOUND_DIR'] = str(sounds)
    os.execve(str(binary), [str(binary), str(data / 'boot-linked.bin'), '--app0', str(game),
              '--user', str(data / 'user'), '--content-profile', str(data / 'content.bin'),
              '--timeout', '0', *runtime_args], env)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Valkyria: {error}', file=sys.stderr)
        sys.exit(1)
