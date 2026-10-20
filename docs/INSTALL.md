# Installing Might and Magic VII (OpenEnroth) on a PS5

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
4. **Might and Magic VII: For Blood and Honor** appears on the home screen within a few seconds. If it doesn't, restart
   ShadowMountPlus.

To update to a newer release, run the installer from the new download with `--program-only`; your game files
and saves stay where they are.

## Controls (current)

Controller support is still in progress.

| Button | In the game | In menus and dialogs |
| --- | --- | --- |
| Left stick | Walk, strafe | |
| Right stick | Turn, look up and down | |
| Cross | Interact: talk, open, pick up | Click what the cursor is on |
| Circle | Game menu: save, load, quit | Back: close the current screen, No |
| Square | Attack | Load Game (game menu) |
| Triangle | Jump; fly up while flying | Save Game, Credits |
| L1 | Spellbook | |
| R1 | Cast the quick spell | Spellbook: make the selected spell the quick spell; Options (main menu); Rest 8 hours (rest menu) |
| L2 | Fly down | Right-click what the cursor is on: hold for item and spell info |
| R2 | Turn-based mode on/off | |
| L3 (press left stick) | Always run on/off | |
| R3 (press right stick) | Pass the turn (turn-based mode) | |
| Options | Inventory | |
| D-pad | Down: next character; Up: notes; Left: map; Right: quests | Move the cursor to the next button or item; hold to keep moving |
| Touchpad | Mouse cursor, see below | Mouse cursor |

**The touchpad is the mouse**, like a laptop's: slide a finger to move the cursor, press the pad to click, and
press it with two fingers on it to right-click (hold for the info pop-ups). In the game view the cursor appears when you
touch the pad and hides again after a few seconds; in menus it stays. Use it for the inventory, spellbook, shops and anything else that needs a
mouse.

To **rest**, click the campfire button at the bottom of the screen. The Create button can't be used: the console
keeps it for itself.

**In menus** (the inventory, the spellbook, shops, the game menu and so on) the D-pad moves the cursor to the nearest
button, inventory cell, equipped item or shop item in that direction, and Cross clicks it. Hold a direction to keep
moving. Hold L2 on an item to see its
details. When the cursor isn't on anything, Cross does the menu's shortcut instead (for example it starts a new game
in the game menu).

Character creation, conversations and the menus inside buildings keep their own D-pad controls: the D-pad moves the
highlight and Cross picks it. In character creation, L1 and R1 lower and raise the highlighted stat, Triangle
clears and Cross finishes.

To **set the quick spell**, open the spellbook (L1), move to a spell, press Cross to select it, and press R1. R1 then
casts it in the game. Pressing Cross again on the selected spell casts it straight away.

To **save**, open the game menu (Circle), choose Save, click an empty slot, then click Save. The name starts as your
location; click Save again to keep it.

Known problems: the sticks work like keys (walk or don't, move at one speed) rather than analog.

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
