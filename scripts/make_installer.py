"""Packages TMIXTOOL into a single-file Windows installer.

Uses IExpress, which ships with Windows. Inno Setup and NSIS would be the
usual choices, but both are distributed only through GitHub releases, which is
unreachable from this machine; IExpress needs nothing downloaded.

The thing being packaged is a compiled installer (installer/setup.cpp) rather
than a script. A PowerShell installer would have to run with the execution
policy bypassed on whatever machine it lands on, which is not a concession an
installer should be asking for.

Output: installer/out/TMIXTOOL-Setup-<version>.exe
"""

import os
import shutil
import subprocess
import sys

VERSION = "1.1"
LAUNCHER = "setup.exe"

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INSTALLER_DIR = os.path.join(ROOT, "installer")
BUILD_DIR = os.path.join(INSTALLER_DIR, "build_package")
PAYLOAD = os.path.join(BUILD_DIR, "payload")
OUT_DIR = os.path.join(INSTALLER_DIR, "out")


def prepare_payload():
    """Lays out what the package contains.

    install.ps1 is gone; setup.exe looks for the application in a sibling `app`
    folder and copies itself in as the uninstaller.
    """
    if os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)
    os.makedirs(PAYLOAD)

    app_source = os.path.join(ROOT, "dist", "TMIXTOOL")
    if not os.path.isdir(app_source):
        sys.exit(f"portable build missing: {app_source}\nrun scripts/package.ps1 first")
    shutil.copytree(app_source, os.path.join(PAYLOAD, "app"))

    setup = os.path.join(ROOT, "build", "installer", "TMIXSetup.exe")
    if not os.path.exists(setup):
        sys.exit(f"installer not built: {setup}\nrun build.bat first")
    shutil.copy2(setup, os.path.join(PAYLOAD, LAUNCHER))


def collect_files():
    """Every file IExpress must pack, relative to the payload."""
    found = []
    for base, _dirs, names in os.walk(PAYLOAD):
        for name in names:
            found.append(os.path.relpath(os.path.join(base, name), PAYLOAD))
    return sorted(found)


def write_sed(files, target):
    """Writes the IExpress directive file.

    Kept ASCII-only: IExpress reads this in the system ANSI code page, so any
    Chinese here would come out mangled in the extraction dialog. The Chinese
    belongs in the installer's own window, which is a compiled resource.
    """
    lines = [
        "[Version]",
        "Class=IEXPRESS",
        "SEDVersion=3",
        "[Options]",
        "PackagePurpose=InstallApp",
        "ShowInstallProgramWindow=1",
        "HideExtractAnimation=0",
        "UseLongFileName=1",
        "InsideCompressed=0",
        "CAB_FixedSize=0",
        "CAB_ResvCodeSigning=0",
        "RebootMode=N",
        "InstallPrompt=",
        "DisplayLicense=",
        "FinishMessage=",
        f"TargetName={target}",
        "FriendlyName=TMIXTOOL Setup",
        f"AppLaunched={LAUNCHER}",
        "PostInstallCmd=<None>",
        "AdminQuietInstCmd=",
        "UserQuietInstCmd=",
        "SourceFiles=SourceFiles",
        "[SourceFiles]",
        f"SourceFiles0={PAYLOAD}\\",
        "[SourceFiles0]",
    ]

    lines += [f"%FILE{i}%=" for i in range(len(files))]

    lines.append("[Strings]")
    lines += [f'FILE{i}="{name}"' for i, name in enumerate(files)]

    path = os.path.join(BUILD_DIR, "tmixtool.sed")
    with open(path, "w", encoding="ascii", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    return path


def main():
    prepare_payload()
    files = collect_files()
    print(f"payload: {len(files)} files")

    os.makedirs(OUT_DIR, exist_ok=True)
    target = os.path.join(OUT_DIR, f"TMIXTOOL-Setup-{VERSION}.exe")

    sed = write_sed(files, target)
    iexpress = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"),
                            "System32", "iexpress.exe")
    if not os.path.exists(iexpress):
        sys.exit(f"iexpress not found at {iexpress}")

    result = subprocess.run([iexpress, "/N", "/Q", sed], capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr)
        sys.exit(f"iexpress failed with exit code {result.returncode}")

    if not os.path.exists(target):
        sys.exit("iexpress reported success but produced no file")

    size = os.path.getsize(target) / 1e6
    print(f"\ninstaller -> {target}  ({size:.1f} MB)")


if __name__ == "__main__":
    main()
