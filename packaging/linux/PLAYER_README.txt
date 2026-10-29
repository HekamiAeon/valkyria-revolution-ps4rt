Valkyria Revolution - PS4RT Linux x86-64

FIRST LAUNCH
Supply your own extracted plaintext PS4 game directory (CUSA06810).
Open a terminal in the extracted player release folder and run:
  bash Play-Game.sh --game "/absolute/path/to/CUSA06810"

SUBSEQUENT LAUNCHES
The launcher remembers the validated game location. Run:
  bash Play-Game.sh
The --game argument remains optional but may be supplied at any time to select
and remember a different valid CUSA06810 directory.

OPTIONAL PRIVATE DATA DIRECTORY
To store saves, settings, and prepared data in a different location:
  bash Play-Game.sh --game "/absolute/path/to/CUSA06810" --data-dir "/path/to/private-data"
When using a custom --data-dir, pass the same value on later launches.
Do not delete or move your existing data directory to change the game path.

TROUBLESHOOTING
If the game directory has moved or the launcher cannot find it, pass --game again.
Select the extracted CUSA06810 directory, not its parent, a PKG, or eboot.bin.
Native Linux x86-64, Python 3, glibc 2.35+, and a working Vulkan driver are required.
On SteamOS, add Play-Game.sh as a non-Steam game from Desktop Mode
(select All Files when necessary). Do not force Windows Proton compatibility.
If first launch has not been configured, run once from a terminal with --game.

Instructions: https://github.com/HekamiAeon/valkyria-revolution-ps4rt
Requires your own legally obtained PS4 game data; no game files are included.
