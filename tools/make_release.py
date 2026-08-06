"""Build a Release OptiNotch.exe and zip it into dist/.

Fonts are embedded in the exe (#embed), so the release is a single
self-contained file that needs no assets/ folder at runtime.

Run from the repo root:
    python tools/make_release.py
"""
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build-release"
DIST = ROOT / "dist"

CMAKE = r"C:\Program Files\CMake\bin\cmake.exe"
CXX = r"C:\Users\AymanCassim\llvm-mingw\llvm-mingw-20260616-msvcrt-x86_64\bin\clang++.exe"
MAKE = r"C:\Users\AymanCassim\llvm-mingw\llvm-mingw-20260616-msvcrt-x86_64\bin\mingw32-make.exe"

VERSION = "0.1.0"


def build_release():
    if not (BUILD / "CMakeCache.txt").exists():
        subprocess.run(
            [
                CMAKE, "-B", str(BUILD), "-G", "MinGW Makefiles",
                f"-DCMAKE_CXX_COMPILER={CXX}",
                f"-DCMAKE_MAKE_PROGRAM={MAKE}",
                "-DCMAKE_BUILD_TYPE=Release",
            ],
            check=True,
        )
    subprocess.run([CMAKE, "--build", str(BUILD), "--config", "Release"], check=True)


def find_bundled_creds():
    """Locate the real OAuth client JSON to ship next to the exe.

    Bundled credentials let end users sign in with their own Google account
    without creating anything in Google Cloud. The file is git-ignored, so the
    release is built from the owner's local copy.
    """
    candidates = [
        ROOT / "gcal_credentials.json",               # dev/owner copy in repo root
        Path(os.environ.get("APPDATA", "")) / "OptiNotch" / "gcal_credentials.json",
    ]
    for p in candidates:
        if p.exists():
            return p
    return None


def package():
    exe = BUILD / "OptiNotch.exe"
    if not exe.exists():
        print(f"error: {exe} not found after build", file=sys.stderr)
        sys.exit(1)

    out_dir = DIST / f"OptiNotch-{VERSION}"
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)

    shutil.copy2(exe, out_dir / "OptiNotch.exe")
    shutil.copy2(ROOT / "tools" / "gcal_credentials.example.json",
                 out_dir / "gcal_credentials.example.json")
    shutil.copy2(ROOT / "tools" / "README.txt", out_dir / "README.txt")

    creds = find_bundled_creds()
    if creds is not None:
        shutil.copy2(creds, out_dir / "gcal_credentials.json")
        print(f"bundled credentials from {creds}")
    else:
        print("warning: no gcal_credentials.json found to bundle "
              "(users will need to import their own via the wizard)",
              file=sys.stderr)

    zip_path = DIST / f"OptiNotch-{VERSION}.zip"
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for f in out_dir.rglob("*"):
            if f.is_file():
                z.write(f, f.relative_to(out_dir))
    print(f"packaged -> {zip_path}")


if __name__ == "__main__":
    build_release()
    package()
