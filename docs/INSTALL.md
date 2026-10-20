# Installing OpenEnroth (Might and Magic VII) on a PS5

## What you need

- A PS5 that runs homebrew native titles:
  - firmware **13.60 or lower** with a working jailbreak (for example Relapse);
  - **kstuff** (or etaHEN with kstuff), so fake-signed titles can run;
  - **ShadowMountPlus**, which turns folders in `/data/homebrew` into home-screen titles;
  - an **FTP server** on the console: the ftpsrv payload (port 2121) or etaHEN's.

  This build was tested on firmware 13.60 with etaHEN, kstuff-lite and ShadowMountPlus.
- **Your own copy of Might and Magic VII**: the GOG or Steam release, installed on a PC. The installer copies its
  `DATA`, `Anims`, `SOUNDS` and `Music` folders (about 580 MB) to the console. Nothing from the game is included
  in this download.
- **Python 3** on the PC (Windows, macOS or Linux).
- The PC and the PS5 on the same network, and the PS5's IP address (Settings › Network › Connection Status).

## Install

1. Start your jailbreak and its payloads on the PS5 as usual.
2. Unzip this download and open a terminal in its folder.
3. Run, with your PS5's address and your game folder (the one that contains `DATA`):

   ```
   python3 install.py --ps5 192.168.1.100 --mm7 "C:\GOG Games\Might and Magic 7"
   ```

   It uploads the title to `/data/homebrew/PPSA99967` and then your game files. Running it again only sends
   what changed.
4. **OpenEnroth (Might and Magic VII)** appears on the home screen within a few seconds. If it doesn't, restart
   ShadowMountPlus.

To update to a newer release, run the installer from the new download with `--program-only`; your game files
and saves stay where they are.

## Controls (current)

Controller support is still in progress. What works today is OpenEnroth's built-in gamepad mapping:

| | |
| --- | --- |
| Main menu | Cross: New Game, Square: Load Game, Triangle: Credits, Circle: Exit |
| Combat | L1: attack, R1: quick-cast, D-pad Down: cast |
| Books | D-pad Up: notes, Left: map, Right: quests |
| Other | Triangle: jump, Options: turn-based mode, Create: rest |

Known problems: the sticks don't move or turn yet, and there is no cursor, so inventory, spellbook, shops and
dialogue need more than the controller can currently do.

## Saves and settings

They live in the title's own storage on the console (`/download0/.openenroth`), separate from the title folder,
so updating the title keeps them. Deleting the title from the console also deletes them.

## If it doesn't start

- **"Can't start the game or app" (CE-107750-0)**: the title's files must be readable by everyone. The installer
  sets that where the FTP server allows it; if you copied the files another way, set the `PPSA99967` folder and
  everything in it to permissions 777.
- **Crashes back to the home screen**: please report it with the console's kernel log. See "Debugging" in the
  project's README.
