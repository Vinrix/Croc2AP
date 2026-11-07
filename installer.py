#!/usr/bin/env python3
"""
Croc 2 Archipelago Randomizer — Mod Installer, Uninstaller & Vanilla Mode Toggler.

Handles:
1. Installing `version.dll` (`croc2_ap_hook.dll`) and `croc2_mod.ini` into the Croc 2 Steam directory.
2. Installing `croc2.apworld` into `C:\\ProgramData\\Archipelago\\custom_worlds\\` and
   `Croc 2.yaml` into `C:\\ProgramData\\Archipelago\\Players\\Templates\\`.
3. Safely backing up and restoring any pre-existing `version.dll` on uninstall.
4. Toggling `enabled = true / false` in `croc2_mod.ini` for instant Vanilla Mode switching.
"""

import argparse
import hashlib
import os
import re
import shutil
import sys
from typing import Dict, List, Optional, Tuple

ROOT_DIR = os.path.abspath(os.path.dirname(__file__))

DEFAULT_GAME_DIRS = [
    r"C:\Program Files (x86)\Steam\steamapps\common\Croc 2",
    r"C:\Program Files\Steam\steamapps\common\Croc 2",
]

DEFAULT_AP_DIRS = [
    r"C:\ProgramData\Archipelago",
    os.path.expandvars(r"%LOCALAPPDATA%\Archipelago"),
]


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def detect_game_dir(custom_dir: Optional[str] = None) -> Optional[str]:
    if custom_dir:
        return os.path.abspath(custom_dir)

    for candidate in DEFAULT_GAME_DIRS:
        if os.path.isfile(os.path.join(candidate, "croc2.exe")):
            return candidate

    # Check Steam libraryfolders.vdf if present
    vdf_candidates = [
        r"C:\Program Files (x86)\Steam\steamapps\libraryfolders.vdf",
        r"C:\Program Files\Steam\steamapps\libraryfolders.vdf",
    ]
    for vdf in vdf_candidates:
        if os.path.isfile(vdf):
            try:
                with open(vdf, "r", encoding="utf-8", errors="ignore") as f:
                    text = f.read()
                for match in re.finditer(r'"path"\s+"([^"]+)"', text):
                    lib_path = match.group(1).replace("\\\\", "\\")
                    candidate = os.path.join(lib_path, "steamapps", "common", "Croc 2")
                    if os.path.isfile(os.path.join(candidate, "croc2.exe")):
                        return candidate
            except OSError:
                pass
    return DEFAULT_GAME_DIRS[0]


def detect_ap_dir(custom_dir: Optional[str] = None) -> str:
    if custom_dir:
        return os.path.abspath(custom_dir)
    for candidate in DEFAULT_AP_DIRS:
        if os.path.isdir(candidate):
            return candidate
    return DEFAULT_AP_DIRS[0]


def build_apworld(dist_path: Optional[str] = None) -> str:
    """
    Packs the `worlds/croc2` directory into a standard Archipelago `.apworld` zip archive.
    """
    import zipfile
    if dist_path is None:
        dist_path = os.path.join(ROOT_DIR, "dist", "croc2.apworld")
    os.makedirs(os.path.dirname(dist_path), exist_ok=True)
    worlds_dir = os.path.join(ROOT_DIR, "worlds", "croc2")
    with zipfile.ZipFile(dist_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for root, dirs, files in os.walk(worlds_dir):
            if "__pycache__" in root:
                continue
            for file in sorted(files):
                if file.endswith(".pyc"):
                    continue
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, os.path.join(ROOT_DIR, "worlds"))
                arcname = rel_path.replace("\\", "/")
                zf.write(full_path, arcname)
    return dist_path


def install_mod(
    game_dir: str,
    ap_dir: str,
    overwrite_ini: bool = False,
    skip_ap: bool = False,
) -> Dict[str, str]:
    """
    Installs the Croc 2 hook DLL, mod config INI, `.apworld` package, and YAML template.
    Returns a summary dictionary of installed file paths.
    """
    installed: Dict[str, str] = {}

    src_dll = os.path.join(ROOT_DIR, "build", "version.dll")
    if not os.path.isfile(src_dll):
        src_dll = os.path.join(ROOT_DIR, "build", "croc2_ap_hook.dll")
    if not os.path.isfile(src_dll):
        raise FileNotFoundError(f"Compiled hook DLL not found at {src_dll}")

    src_ini = os.path.join(ROOT_DIR, "croc2_mod.ini")
    src_apworld = os.path.join(ROOT_DIR, "dist", "croc2.apworld")
    if not os.path.isfile(src_apworld):
        build_apworld(src_apworld)
    src_yaml = os.path.join(ROOT_DIR, "Croc 2.yaml")

    os.makedirs(game_dir, exist_ok=True)
    dst_dll = os.path.join(game_dir, "version.dll")
    backup_dll = os.path.join(game_dir, "version.dll.vanilla_backup")

    # Back up existing version.dll only if it is NOT already our Croc2AP hook DLL
    if os.path.isfile(dst_dll) and not os.path.isfile(backup_dll):
        with open(dst_dll, "rb") as f:
            existing_bytes = f.read()
        if b"Croc2AP_Initialize" not in existing_bytes:
            shutil.copy2(dst_dll, backup_dll)
            installed["backup_dll"] = backup_dll

    shutil.copy2(src_dll, dst_dll)
    installed["game_dll"] = dst_dll

    dst_ini = os.path.join(game_dir, "croc2_mod.ini")
    if overwrite_ini or not os.path.isfile(dst_ini):
        shutil.copy2(src_ini, dst_ini)
    installed["game_ini"] = dst_ini

    src_launcher = os.path.join(ROOT_DIR, "Croc2ArchipelagoLauncher.exe")
    if os.path.isfile(src_launcher):
        dst_launcher = os.path.join(game_dir, "Croc2ArchipelagoLauncher.exe")
        shutil.copy2(src_launcher, dst_launcher)
        installed["launcher"] = dst_launcher

    src_setup = os.path.join(ROOT_DIR, "Croc2AP_Setup.exe")
    if os.path.isfile(src_setup):
        dst_setup = os.path.join(game_dir, "Croc2AP_Setup.exe")
        shutil.copy2(src_setup, dst_setup)
        installed["setup_exe"] = dst_setup

    src_instr = os.path.join(ROOT_DIR, "INSTALL_INSTRUCTIONS.txt")
    if os.path.isfile(src_instr):
        shutil.copy2(src_instr, os.path.join(game_dir, "INSTALL_INSTRUCTIONS.txt"))

    src_ico = os.path.join(ROOT_DIR, "icon.ico")
    if os.path.isfile(src_ico):
        shutil.copy2(src_ico, os.path.join(game_dir, "icon.ico"))

    if not skip_ap:
        custom_worlds_dir = os.path.join(ap_dir, "custom_worlds")
        templates_dir = os.path.join(ap_dir, "Players", "Templates")
        os.makedirs(custom_worlds_dir, exist_ok=True)
        os.makedirs(templates_dir, exist_ok=True)

        dst_apworld = os.path.join(custom_worlds_dir, "croc2.apworld")
        shutil.copy2(src_apworld, dst_apworld)
        installed["apworld"] = dst_apworld

        dst_yaml = os.path.join(templates_dir, "Croc 2.yaml")
        shutil.copy2(src_yaml, dst_yaml)
        installed["yaml_template"] = dst_yaml

    return installed


def uninstall_mod(game_dir: str, ap_dir: str, skip_ap: bool = False) -> List[str]:
    """
    Cleanly removes the Croc 2 hook DLL, mod config INI, `.apworld`, and YAML template,
    and restores any backed-up vanilla `version.dll`.
    """
    removed: List[str] = []

    dst_dll = os.path.join(game_dir, "version.dll")
    extra_dll = os.path.join(game_dir, "croc2_ap_hook.dll")
    backup_dll = os.path.join(game_dir, "version.dll.vanilla_backup")
    dst_ini = os.path.join(game_dir, "croc2_mod.ini")
    dst_launcher = os.path.join(game_dir, "Croc2ArchipelagoLauncher.exe")

    for target in (dst_dll, extra_dll, dst_ini, dst_launcher):
        if os.path.isfile(target):
            os.remove(target)
            removed.append(target)

    if os.path.isfile(backup_dll):
        shutil.move(backup_dll, dst_dll)
        removed.append(f"Restored {dst_dll} from backup")

    if not skip_ap:
        dst_apworld = os.path.join(ap_dir, "custom_worlds", "croc2.apworld")
        dst_yaml = os.path.join(ap_dir, "Players", "Templates", "Croc 2.yaml")
        for target in (dst_apworld, dst_yaml):
            if os.path.isfile(target):
                os.remove(target)
                removed.append(target)

    return removed


def toggle_vanilla_ini(game_dir: str, force_state: Optional[bool] = None) -> Tuple[bool, str]:
    """
    Toggles `enabled = true / false` in `<game_dir>/croc2_mod.ini`.
    Returns `(new_enabled_state, ini_path)`.
    """
    ini_path = os.path.join(game_dir, "croc2_mod.ini")
    if not os.path.isfile(ini_path):
        src_ini = os.path.join(ROOT_DIR, "croc2_mod.ini")
        os.makedirs(game_dir, exist_ok=True)
        shutil.copy2(src_ini, ini_path)

    with open(ini_path, "r", encoding="utf-8") as f:
        content = f.read()

    current_enabled = True
    m = re.search(r"(?m)^(\s*enabled\s*=\s*)(true|false|1|0)\s*$", content, re.IGNORECASE)
    if m:
        current_enabled = m.group(2).lower() in ("true", "1")

    new_enabled = (not current_enabled) if force_state is None else force_state
    new_str = "true" if new_enabled else "false"

    if m:
        content = re.sub(
            r"(?m)^(\s*enabled\s*=\s*)(true|false|1|0)\s*$",
            rf"\g<1>{new_str}",
            content,
            flags=re.IGNORECASE,
        )
    else:
        content += f"\nenabled = {new_str}\n"

    with open(ini_path, "w", encoding="utf-8") as f:
        f.write(content)

    return new_enabled, ini_path


def get_install_status(game_dir: str, ap_dir: str) -> Dict[str, object]:
    dll_path = os.path.join(game_dir, "version.dll")
    ini_path = os.path.join(game_dir, "croc2_mod.ini")
    apworld_path = os.path.join(ap_dir, "custom_worlds", "croc2.apworld")
    yaml_path = os.path.join(ap_dir, "Players", "Templates", "Croc 2.yaml")
    launcher_path = os.path.join(game_dir, "Croc2ArchipelagoLauncher.exe")

    ini_enabled = None
    if os.path.isfile(ini_path):
        with open(ini_path, "r", encoding="utf-8") as f:
            m = re.search(r"(?m)^\s*enabled\s*=\s*(true|false|1|0)\s*$", f.read(), re.IGNORECASE)
            if m:
                ini_enabled = m.group(1).lower() in ("true", "1")

    return {
        "game_dir": game_dir,
        "ap_dir": ap_dir,
        "hook_dll_installed": os.path.isfile(dll_path),
        "mod_ini_installed": os.path.isfile(ini_path),
        "mod_ini_enabled": ini_enabled,
        "apworld_installed": os.path.isfile(apworld_path),
        "yaml_template_installed": os.path.isfile(yaml_path),
        "launcher_installed": os.path.isfile(launcher_path),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Croc 2 Archipelago Randomizer — Mod Installer & Uninstaller"
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--install", action="store_true", help="Install the Croc 2 Mod and Archipelago World")
    group.add_argument("--uninstall", action="store_true", help="Uninstall the Croc 2 Mod and restore vanilla state")
    group.add_argument("--toggle-vanilla", action="store_true", help="Toggle enabled=true/false in croc2_mod.ini")
    group.add_argument("--status", action="store_true", help="Display current installation status")
    group.add_argument("--build-apworld", action="store_true", help="Pack worlds/croc2 into dist/croc2.apworld")

    parser.add_argument("--game-dir", default=None, help="Path to Croc 2 folder (auto-detected by default)")
    parser.add_argument("--ap-dir", default=None, help="Path to Archipelago folder (default: C:\\ProgramData\\Archipelago)")
    parser.add_argument("--skip-ap", action="store_true", help="Only install/uninstall game folder files, skip Archipelago folder")
    parser.add_argument("--overwrite-ini", action="store_true", help="Overwrite existing croc2_mod.ini on install")

    args = parser.parse_args()
    game_dir = detect_game_dir(args.game_dir)
    ap_dir = detect_ap_dir(args.ap_dir)

    try:
        if args.build_apworld:
            target = build_apworld()
            print(f"[Installer] Successfully built .apworld package: {target}")
            return 0

        if args.status:
            status = get_install_status(game_dir, ap_dir)
            print("=== Croc 2 Archipelago Randomizer Status ===")
            for k, v in status.items():
                print(f"  {k}: {v}")
            return 0

        if args.install:
            print(f"[Installer] Target Croc 2 Directory:      {game_dir}")
            if not args.skip_ap:
                print(f"[Installer] Target Archipelago Directory: {ap_dir}")
            installed = install_mod(
                game_dir=game_dir,
                ap_dir=ap_dir,
                overwrite_ini=args.overwrite_ini,
                skip_ap=args.skip_ap,
            )
            print("[Installer] Successfully installed Croc 2 Archipelago Randomizer:")
            for label, path in installed.items():
                print(f"  + {label}: {path}")
            return 0

        if args.uninstall:
            print(f"[Uninstaller] Cleaning Croc 2 Directory:      {game_dir}")
            if not args.skip_ap:
                print(f"[Uninstaller] Cleaning Archipelago Directory: {ap_dir}")
            removed = uninstall_mod(game_dir=game_dir, ap_dir=ap_dir, skip_ap=args.skip_ap)
            if removed:
                print("[Uninstaller] Removed mod files:")
                for path in removed:
                    print(f"  - {path}")
            else:
                print("[Uninstaller] No mod files were present; directory is already clean.")
            return 0

        if args.toggle_vanilla:
            enabled, ini_path = toggle_vanilla_ini(game_dir)
            mode_str = "Archipelago Randomizer ENABLED" if enabled else "Vanilla Mode FORCED (Mod Disabled)"
            print(f"[Toggler] Updated {ini_path} -> enabled = {str(enabled).lower()} ({mode_str})")
            return 0

    except PermissionError as exc:
        print(f"[Error] Permission denied: {exc}", file=sys.stderr)
        print("Tip: Run Install_Croc2AP.bat / Uninstall_Croc2AP.bat (which automatically request Administrator privileges).", file=sys.stderr)
        return 2
    except Exception as exc:
        print(f"[Error] {exc}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())

