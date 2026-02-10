#!/usr/bin/env python3

import subprocess
import sys
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"

def clean():
    if BUILD_DIR.exists():
        print(f"Removing {BUILD_DIR}")
        shutil.rmtree(BUILD_DIR)
    else:
        print("Nothing to clean")

def run(cmd, cwd=None):
    print(">>", " ".join(cmd))
    subprocess.check_call(cmd, cwd=cwd)


def build(build_type: str):
    outdir = BUILD_DIR / build_type.lower()
    outdir.mkdir(parents=True, exist_ok=True)

    print(f"\n=== Building {build_type} ===")
    run([
        "cmake",
        "-S", str(ROOT),
        "-B", str(outdir),
        f"-DCMAKE_BUILD_TYPE={build_type}"
    ])

    run([
        "cmake",
        "--build", str(outdir),
        "-j"
    ])

    print(f"=== Done: {outdir / 'SimpleChat'} ===\n")


def main():
    if len(sys.argv) == 1:
        build("Release")
        return

    for arg in sys.argv[1:]:
        arg = arg.lower()
        if arg == "clean":
            clean()
        elif arg == "debug":
            build("Debug")
        elif arg == "release":
            build("Release")
        elif arg == "all":
            build("Release")
            build("Debug")
        else:
            print(f"Unknown command: {arg}")
            sys.exit(1)


if __name__ == "__main__":
    main()