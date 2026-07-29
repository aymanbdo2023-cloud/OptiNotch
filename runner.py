import subprocess
import sys
from pathlib import Path
import ctypes
import os
import time

BUILD_DIR = Path("build")
DLL_PATH = BUILD_DIR / "libOptiNotch_shared.dll"

mtime = 0


def build():
    if not (BUILD_DIR / "CMakeCache.txt").exists():
        subprocess.run([
            "C:\\Program Files\\CMake\\bin\\cmake.exe", "-B", "build",
            "-G", "MinGW Makefiles",
            "-DCMAKE_CXX_COMPILER=C:\\Users\\AymanCassim\\llvm-mingw\\llvm-mingw-20260616-msvcrt-x86_64\\bin\\clang++.exe",
            "-DCMAKE_MAKE_PROGRAM=C:\\Users\\AymanCassim\\llvm-mingw\\llvm-mingw-20260616-msvcrt-x86_64\\bin\\mingw32-make.exe"
        ], check=True)
    subprocess.run([
        "C:\\Program Files\\CMake\\bin\\cmake.exe", "--build", "build"
    ], check=True)

def run():
    dll = ctypes.CDLL(str(DLL_PATH))
    dll.run.restype = ctypes.c_int
    ret = dll.run()
    print(f"run() returned: {ret}")
    sys.exit(ret)

if __name__ == "__main__":
    build()
    os.chdir(Path(__file__).parent)
    run()
