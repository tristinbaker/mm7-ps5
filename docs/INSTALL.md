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

Controller support is still in progress.

| Button | In the game | In menus and dialogs |
| --- | --- | --- |
| Left stick | Walk, strafe | |
| Right stick | Turn, look up and down | |
| Cross | Interact: talk, open, pick up | Select, New Game |
| Circle | Back / game menu | Back, Exit |
| Square | Attack | Load Game |
| Triangle | Jump; fly up while flying | Save Game, Credits |
| L1 | Spellbook | |
| R1 | Quick spell | Options |
| L2 | Fly down | |
| R2 | Turn-based mode on/off | |
| L3 (press left stick) | Always run on/off | |
| R3 (press right stick) | Pass the turn (turn-based mode) | |
| Options | Inventory | |
| Create | Rest | Rest 8 hours (in the rest menu) |
| D-pad | Down: next character; Up: notes; Left: map; Right: quests | Move between choices |

Known problems: there is no cursor yet, so the inventory, spellbook, shops and parts of dialogs need more than the
controller can currently do. The sticks work like keys (walk or don't, turn at one speed) rather than analog.

The layout is written to the game's configuration when the title starts. You can change bindings in the game's
controls menu; a release that changes the layout replaces the gamepad bindings once.

## Saves and settings

They live in the title's own storage on the console (`/download0/.openenroth`), separate from the title folder,
so updating the title keeps them. Deleting the title from the console also deletes them.

## If it doesn't start

- **"Can't start the game or app" (CE-107750-0)**: the title's files must be readable by everyone. The installer
  sets that where the FTP server allows it; if you copied the files another way, set the `PPSA99967` folder and
  everything in it to permissions 777.
- **Crashes back to the home screen**: please report it with the console's kernel log. See "Debugging" in the
  project's README.
