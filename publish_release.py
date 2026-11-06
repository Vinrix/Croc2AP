#!/usr/bin/env python3
"""
Croc 2 Archipelago Randomizer — Release Publishing & GitHub Sync Automation.

This script automates the complete release lifecycle:
1. Validates or updates the semantic version in `src/launcher/UpdateManager.cs`.
2. Compiles the C++ in-game hook DLLs (`croc2_ap_hook.dll` & `version.dll`) via MinGW-w64.
3. Compiles the standalone launcher (`Croc2ArchipelagoLauncher.exe`) via Microsoft .NET csc.exe.
4. Packs the `worlds/croc2` directory into `dist/croc2.apworld`.
5. Executes the automated test suite (`tests/test_croc2_ap.py`) to verify logic integrity.
6. Bundles release assets into `dist/Croc2Archipelago-v{version}.zip`.
7. Performs Git operations: commit, tag `v{version}`, and push to GitHub.
8. If GitHub CLI (`gh`) is available, automatically publishes a GitHub Release with all assets.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from typing import Optional, Tuple

ROOT_DIR = os.path.abspath(os.path.dirname(__file__))
UPDATE_MANAGER_PATH = os.path.join(ROOT_DIR, "src", "launcher", "UpdateManager.cs")
DEFAULT_REPO = "Vinrix/Croc2AP"


def print_banner(text: str) -> None:
    line = "=" * 64
    print(f"\n{line}\n  {text}\n{line}")


def run_command(cmd: str, env_extra: Optional[dict] = None) -> None:
    print(f"\n>>> Running: {cmd}")
    env = os.environ.copy()
    if env_extra:
        env.update(env_extra)

    # Ensure MinGW and MSYS2 are on PATH for compiler tools
    mingw_paths = r"C:\msys64\mingw64\bin;C:\msys64\usr\bin;"
    if "PATH" in env and "msys64" not in env["PATH"]:
        env["PATH"] = mingw_paths + env["PATH"]

    ps_args = ["powershell.exe", "-NoProfile", "-Command", cmd]
    result = subprocess.run(ps_args, stdin=subprocess.DEVNULL, env=env, cwd=ROOT_DIR)
    if result.returncode != 0:
        print(f"\n[ERROR] Command failed with exit code {result.returncode}!", file=sys.stderr)
        sys.exit(result.returncode)


def get_current_version() -> str:
    if not os.path.isfile(UPDATE_MANAGER_PATH):
        return "1.0.0"
    with open(UPDATE_MANAGER_PATH, "r", encoding="utf-8") as f:
        content = f.read()
    match = re.search(r'public const string CurrentVersion\s*=\s*"([^"]+)";', content)
    return match.group(1) if match else "1.0.0"


def set_version(new_version: str) -> None:
    new_version = new_version.strip().lstrip("vV")
    with open(UPDATE_MANAGER_PATH, "r", encoding="utf-8") as f:
        content = f.read()
    new_content = re.sub(
        r'public const string CurrentVersion\s*=\s*"[^"]+";',
        f'public const string CurrentVersion = "{new_version}";',
        content,
    )
    with open(UPDATE_MANAGER_PATH, "w", encoding="utf-8") as f:
        f.write(new_content)
    print(f"[Version] Updated {UPDATE_MANAGER_PATH} -> v{new_version}")


def bump_version(current: str, part: str) -> str:
    parts = [int(p) for p in current.split(".")]
    while len(parts) < 3:
        parts.append(0)
    if part == "major":
        parts[0] += 1
        parts[1] = 0
        parts[2] = 0
    elif part == "minor":
        parts[1] += 1
        parts[2] = 0
    else:  # patch
        parts[2] += 1
    return f"{parts[0]}.{parts[1]}.{parts[2]}"


def compile_hook() -> None:
    print_banner("1. Compiling C++ In-Game Hook DLLs (MinGW-w64)")
    os.makedirs(os.path.join(ROOT_DIR, "build"), exist_ok=True)
    gxx = r"C:\msys64\mingw64\bin\g++.exe"
    if not os.path.isfile(gxx):
        gxx = "g++"

    cmd = (
        f'& "{gxx}" -shared -std=c++17 -O2 -s -Wall -Wextra -static -static-libgcc -static-libstdc++ '
        f'"src/hook/croc2_ap_hook.cpp" "src/hook/version.def" '
        f'-o "build/croc2_ap_hook.dll" -lws2_32 -lwinhttp -lgdi32 -luser32'
    )
    run_command(cmd)

    src_dll = os.path.join(ROOT_DIR, "build", "croc2_ap_hook.dll")
    dst_dll = os.path.join(ROOT_DIR, "build", "version.dll")
    shutil.copy2(src_dll, dst_dll)
    print(f"[Hook] Successfully built: {src_dll} and {dst_dll}")


def compile_launcher() -> None:
    print_banner("2. Compiling Standalone Launcher (C# 5 / .NET 4.0)")
    cmd = "& cmd.exe /c build.bat"
    run_command(cmd)
    print("[Launcher] Successfully built: Croc2ArchipelagoLauncher.exe")


def build_apworld() -> str:
    print_banner("3. Building croc2.apworld Archive")
    import installer
    apworld_path = installer.build_apworld()
    print(f"[APWorld] Created archive: {apworld_path}")
    return apworld_path


def run_tests() -> None:
    print_banner("4. Running Automated Verification Test Suite")
    python_exe = r"C:\msys64\usr\bin\python.exe" if os.path.isfile(r"C:\msys64\usr\bin\python.exe") else sys.executable
    cmd = f'& "{python_exe}" -m unittest tests/test_croc2_ap.py'
    run_command(cmd)
    print("[Tests] All tests passed with 100% success!")


def package_release_zip(version: str) -> str:
    print_banner("5. Creating Release Distribution ZIP Archive")
    dist_dir = os.path.join(ROOT_DIR, "dist")
    os.makedirs(dist_dir, exist_ok=True)
    zip_filename = f"Croc2Archipelago-v{version}.zip"
    zip_path = os.path.join(dist_dir, zip_filename)

    files_to_pack = [
        ("Croc2ArchipelagoLauncher.exe", "Croc2ArchipelagoLauncher.exe"),
        (os.path.join("build", "version.dll"), "version.dll"),
        ("croc2_mod.ini", "croc2_mod.ini"),
        (os.path.join("dist", "croc2.apworld"), "croc2.apworld"),
        (os.path.join("templates", "Croc 2.yaml"), "Croc 2.yaml"),
        ("Install_Croc2AP.bat", "Install_Croc2AP.bat"),
        ("Uninstall_Croc2AP.bat", "Uninstall_Croc2AP.bat"),
        ("Toggle_Vanilla_Mode.bat", "Toggle_Vanilla_Mode.bat"),
        ("README.md", "README.md"),
    ]

    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for rel_src, arcname in files_to_pack:
            full_src = os.path.join(ROOT_DIR, rel_src)
            if os.path.isfile(full_src):
                zf.write(full_src, arcname)
                print(f"  + Added: {arcname}")
            else:
                print(f"  ! Warning: Missing file {full_src}")

    print(f"\n[Package] Created: {zip_path} ({os.path.getsize(zip_path):,} bytes)")
    return zip_path


def git_operations(version: str, notes: str, push: bool = True) -> None:
    print_banner("6. Git Commit, Tagging & GitHub Synchronization")

    # Check git repository
    if not os.path.isdir(os.path.join(ROOT_DIR, ".git")):
        print("[Git] Initializing local git repository...")
        run_command("git init")
        run_command('git config user.name "Vinrix"')
        run_command('git config user.email "vinrix@example.com"')

    tag = f"v{version}"
    commit_msg = f"Release {tag}: {notes}" if notes else f"Release {tag}"

    run_command("git add .")
    # Commit if there are staged changes
    res = subprocess.run("git diff --cached --quiet", shell=True, cwd=ROOT_DIR)
    if res.returncode != 0:
        run_command(f'git commit -m "{commit_msg}"')
    else:
        print("[Git] Working tree clean; no changes to commit.")

    # Check if tag already exists
    tag_check = subprocess.run(f"git tag -l {tag}", shell=True, capture_output=True, text=True, cwd=ROOT_DIR)
    if tag not in tag_check.stdout:
        run_command(f'git tag -a "{tag}" -m "{commit_msg}"')
        print(f"[Git] Created tag: {tag}")
    else:
        print(f"[Git] Tag {tag} already exists.")

    if push:
        remote_check = subprocess.run("git remote", shell=True, capture_output=True, text=True, cwd=ROOT_DIR)
        if "origin" in remote_check.stdout:
            print("[Git] Pushing commits and tags to origin...")
            run_command("git push origin HEAD --tags")
        else:
            print("\n[Git] No 'origin' remote configured yet.")
            print(f"  To link your GitHub repository, run:")
            print(f"    git remote add origin https://github.com/{DEFAULT_REPO}.git")
            print(f"    git branch -M main")
            print(f"    git push -u origin main --tags")

    # Attempt GitHub Release via gh CLI if present
    gh_check = shutil.which("gh")
    if gh_check and push:
        print("\n[GitHub CLI] Detected gh tool. Attempting to create GitHub Release...")
        zip_path = os.path.join(ROOT_DIR, "dist", f"Croc2Archipelago-v{version}.zip")
        apworld_path = os.path.join(ROOT_DIR, "dist", "croc2.apworld")
        dll_path = os.path.join(ROOT_DIR, "build", "version.dll")
        exe_path = os.path.join(ROOT_DIR, "Croc2ArchipelagoLauncher.exe")

        gh_cmd = (
            f'gh release create {tag} "{zip_path}" "{exe_path}" "{apworld_path}" "{dll_path}" '
            f'--title "Croc 2 Archipelago Randomizer {tag}" '
            f'--notes "{notes or "Croc 2 Archipelago Randomizer and Standalone Launcher release."}"'
        )
        try:
            run_command(gh_cmd)
            print(f"[GitHub CLI] Release {tag} successfully created!")
        except Exception as e:
            print(f"[GitHub CLI] Notice: Could not automatically create release via gh ({e}).")


def main() -> None:
    parser = argparse.ArgumentParser(description="Croc 2 Archipelago — Release & GitHub Sync Automation")
    parser.add_argument("--version", default=None, help="Specific version number (e.g. 1.0.1)")
    parser.add_argument("--bump", choices=["patch", "minor", "major"], default=None, help="Bump semantic version")
    parser.add_argument("--message", "-m", default="", help="Release notes / commit message")
    parser.add_argument("--no-push", action="store_true", help="Skip git push to remote")
    parser.add_argument("--dry-run", action="store_true", help="Only build and test; do not commit or tag")

    args = parser.parse_args()

    current_ver = get_current_version()
    if args.version:
        target_ver = args.version.strip().lstrip("vV")
        set_version(target_ver)
    elif args.bump:
        target_ver = bump_version(current_ver, args.bump)
        set_version(target_ver)
    else:
        target_ver = current_ver

    print_banner(f"Croc 2 Archipelago Release Automation: v{target_ver}")
    print(f"  Target Version:  v{target_ver}")
    print(f"  Release Notes:   {args.message if args.message else '(Default)'}")
    print(f"  Push to Remote:  {not args.no_push}")
    print(f"  Dry Run:         {args.dry_run}")

    # Build sequence
    compile_hook()
    compile_launcher()
    build_apworld()
    run_tests()
    zip_path = package_release_zip(target_ver)

    if not args.dry_run:
        git_operations(target_ver, args.message, push=not args.no_push)

    print_banner(f"SUCCESS: Croc 2 Archipelago v{target_ver} is Ready!")
    print(f"1. Standalone Launcher: Croc2ArchipelagoLauncher.exe (v{target_ver})")
    print(f"2. In-Game Hook DLL:    build/version.dll")
    print(f"3. Archipelago World:   dist/croc2.apworld")
    print(f"4. Release Package:     {zip_path}")
    print("\nWhen pushed to GitHub, players using Croc2ArchipelagoLauncher.exe can simply")
    print("click '🔄 Check for Updates' to automatically download and apply the new release!")


if __name__ == "__main__":
    main()
