#!/usr/bin/env python3
"""Installs the OpenEnroth PS5 title and your Might and Magic VII files on a jailbroken PS5 over FTP.

Needs only Python 3. The console must be running an FTP server (ftpsrv on port 2121, or etaHEN's) and
ShadowMountPlus, which registers the folder under /data/homebrew as a home-screen title.

    python3 install.py --ps5 192.168.1.100 --mm7 "/path/to/Might and Magic 7"

uploads the title folder next to this script (or --title) and the game data. Files that are already on the
console with the same size are skipped, so running it again only sends what changed. Use --program-only to
update just the program, or --data-only to send just the game files.
"""
import argparse
import os
import sys
from ftplib import FTP, error_perm, error_reply
from pathlib import Path

TITLE_ID = "PPSA99967"
REMOTE_ROOT = f"/data/homebrew/{TITLE_ID}"
GAME_DIRECTORIES = ("DATA", "Anims", "SOUNDS", "Music")
REQUIRED_GAME_FILES = ("DATA/GAMES.LOD", "DATA/BITMAPS.LOD", "DATA/SPRITES.LOD", "DATA/ICONS.LOD", "SOUNDS/Audio.snd")


def find_case_insensitive(root: Path, relative: str) -> Path | None:
    path = root
    for part in relative.split("/"):
        if not path.is_dir():
            return None
        match = next((child for child in path.iterdir() if child.name.lower() == part.lower()), None)
        if match is None:
            return None
        path = match
    return path


def check_game_folder(root: Path) -> None:
    missing = [name for name in REQUIRED_GAME_FILES if find_case_insensitive(root, name) is None]
    if missing:
        sys.exit(f"{root} doesn't look like a Might and Magic VII installation, missing: {', '.join(missing)}")


def ensure_directory(ftp: FTP, path: str, known: set) -> None:
    parts = path.strip("/").split("/")
    for i in range(1, len(parts) + 1):
        current = "/" + "/".join(parts[:i])
        if current in known:
            continue
        try:
            ftp.mkd(current)
        except error_perm:
            pass  # Already there.
        known.add(current)


def delete(ftp: FTP, path: str) -> None:
    """Deletes a remote file if it exists. ftpsrv answers DELE with 226 rather than 250, which ftplib rejects."""
    try:
        ftp.sendcmd(f"DELE {path}")
    except error_perm:
        pass  # Not there.
    except error_reply as reply:
        if not str(reply).startswith("2"):
            raise


def remote_size(ftp: FTP, path: str) -> int | None:
    try:
        return ftp.size(path)
    except (error_perm, error_reply):
        return None


def upload(ftp: FTP, local: Path, remote: str, known: set, force: bool, label: str) -> bool:
    """Uploads under a temporary name and renames it into place. Returns False if the file was already there."""
    size = local.stat().st_size
    if not force and remote_size(ftp, remote) == size:
        return False
    ensure_directory(ftp, remote.rsplit("/", 1)[0], known)
    temporary = remote + ".upload"
    delete(ftp, temporary)
    with local.open("rb") as stream:
        ftp.storbinary(f"STOR {temporary}", stream, blocksize=1 << 20)
    delete(ftp, remote)
    ftp.rename(temporary, remote)
    # The console only starts an app whose files are open to everyone. ftpsrv already writes them that way;
    # other FTP servers may not, and some don't support SITE CHMOD at all.
    try:
        ftp.sendcmd(f"SITE CHMOD 777 {remote}")
    except (error_perm, error_reply):
        pass
    print(f"  {label}: {remote} ({size / 1048576:.1f} MiB)")
    return True


def install_program(ftp: FTP, title: Path, known: set) -> None:
    if not (title / "eboot.bin").is_file() or not (title / "sce_sys/param.json").is_file():
        sys.exit(f"{title} is not a title folder (no eboot.bin or sce_sys/param.json)")
    files = sorted(p for p in title.rglob("*") if p.is_file() and not p.relative_to(title).parts[0] == "assets")
    # The loader looks for eboot.bin and param.json, so publish them last.
    last = [title / "eboot.bin", title / "sce_sys/param.json"]
    files = [f for f in files if f not in last] + last
    print(f"Program -> {REMOTE_ROOT}")
    for path in files:
        upload(ftp, path, f"{REMOTE_ROOT}/{path.relative_to(title).as_posix()}", known, force=True, label="sent")


def install_data(ftp: FTP, game: Path, known: set) -> None:
    check_game_folder(game)
    files = []
    for directory in GAME_DIRECTORIES:
        source = find_case_insensitive(game, directory)
        if source is None:
            continue
        # The Direct3D texture caches (*.hwl) are only used by the original game's hardware renderer.
        files += [p for p in sorted(source.rglob("*")) if p.is_file() and p.suffix.lower() != ".hwl"]
    total = sum(p.stat().st_size for p in files)
    print(f"Game data -> {REMOTE_ROOT}/assets/mm7 ({len(files)} files, {total / 1048576:.0f} MiB)")
    for path in files:
        remote = f"{REMOTE_ROOT}/assets/mm7/{path.relative_to(game).as_posix()}"
        if not upload(ftp, path, remote, known, force=False, label="sent"):
            print(f"  have: {remote}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--ps5", required=True, help="IP address of the PS5")
    parser.add_argument("--port", type=int, default=2121, help="FTP port (default 2121, ftpsrv)")
    parser.add_argument("--title", type=Path, default=Path(__file__).resolve().parent / TITLE_ID,
                        help=f"title folder to install (default: {TITLE_ID} next to this script)")
    parser.add_argument("--mm7", type=Path, help="your Might and Magic VII folder (with DATA, Anims, SOUNDS, Music)")
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--program-only", action="store_true", help="only upload the program")
    group.add_argument("--data-only", action="store_true", help="only upload the game data")
    args = parser.parse_args()
    if not args.program_only and args.mm7 is None:
        parser.error("--mm7 is required unless --program-only is given")

    known = set()
    with FTP() as ftp:
        ftp.connect(args.ps5, args.port, timeout=30)
        ftp.login()
        if not args.data_only:
            install_program(ftp, args.title, known)
        if not args.program_only:
            install_data(ftp, args.mm7, known)
    print(f"Done. {TITLE_ID} should appear on the home screen; if it doesn't, restart ShadowMountPlus.")


if __name__ == "__main__":
    main()
