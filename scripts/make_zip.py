"""Builds the distributable archive.

Lays out one named folder rather than spraying files loose, and writes the
archive with Python's zipfile rather than .NET's ZipFile.CreateFromDirectory:
on Windows the latter emits backslashes as path separators, which the zip
specification does not allow. Explorer tolerates it, other extractors produce
files whose names contain the separator.

Chinese file names are written with the UTF-8 name flag set, so they survive
the round trip.

Output: dist/TMIXTOOL-<version>.zip
"""

import os
import shutil
import sys
import zipfile

VERSION = "1.1.3"
FOLDER = "TMIXTOOL"

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist")
SOURCE = os.path.join(DIST, FOLDER)
GUIDE = os.path.join(ROOT, "installer", "使用说明.txt")
STAGE = os.path.join(DIST, "_stage")


def main():
    if not os.path.isdir(SOURCE):
        sys.exit(f"portable build missing: {SOURCE}\nrun scripts/package.ps1 first")

    if os.path.exists(STAGE):
        shutil.rmtree(STAGE)
    staged = os.path.join(STAGE, FOLDER)
    shutil.copytree(SOURCE, staged)

    if os.path.exists(GUIDE):
        shutil.copy2(GUIDE, os.path.join(staged, "使用说明.txt"))

    target = os.path.join(DIST, f"{FOLDER}-{VERSION}.zip")
    if os.path.exists(target):
        os.remove(target)

    count = 0
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for base, directories, names in os.walk(STAGE):
            directories.sort()
            for name in sorted(names):
                full = os.path.join(base, name)
                arcname = os.path.relpath(full, STAGE).replace(os.sep, "/")
                archive.write(full, arcname)
                count += 1

    shutil.rmtree(STAGE)

    print(f"files: {count}")
    print(f"archive -> {target}  ({os.path.getsize(target) / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
