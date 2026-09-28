# Builds a portable, self-contained folder for TMIXTOOL.
#
# The result is a directory that runs on a machine with no Qt and no Visual C++
# redistributable installed: the CRT is linked statically and every Qt
# dependency is copied in beside the executable.

param(
    [string]$QtDir   = 'C:\Qt\6.8.3\msvc2022_64',
    [string]$BuildDir = 'build',
    [string]$OutDir  = 'dist\TMIXTOOL'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root

try {
    $windeployqt = Join-Path $QtDir 'bin\windeployqt.exe'
    if (-not (Test-Path $windeployqt)) {
        throw "windeployqt not found at $windeployqt"
    }

    $guiExe = Join-Path $BuildDir 'src\gui\TMIXTOOL.exe'
    $cliExe = Join-Path $BuildDir 'src\cli\TMIXCLI.exe'

    foreach ($exe in @($guiExe, $cliExe)) {
        if (-not (Test-Path $exe)) {
            throw "missing $exe - build first (build.bat)"
        }
    }

    if (Test-Path $OutDir) {
        Remove-Item $OutDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

    Copy-Item $guiExe $OutDir
    Copy-Item $cliExe $OutDir

    # --qmldir lets the deployment tool scan the QML for imports; without it the
    # QuickControls2 style plugins are missed entirely.
    $qmlDir = Join-Path $root 'src\gui'

    & $windeployqt `
        --release `
        --qmldir $qmlDir `
        --no-translations `
        --no-system-d3d-compiler `
        --compiler-runtime `
        (Join-Path $OutDir 'TMIXTOOL.exe')

    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed with exit code $LASTEXITCODE"
    }

    # windeployqt does not always pick up the style the app selects at runtime,
    # and a missing style plugin is a blank window rather than an error.
    $styleSource = Join-Path $QtDir 'qml\QtQuick\Controls\FluentWinUI3'
    $styleTarget = Join-Path $OutDir 'qml\QtQuick\Controls\FluentWinUI3'

    if (Test-Path $styleSource) {
        New-Item -ItemType Directory -Path $styleTarget -Force | Out-Null
        Copy-Item (Join-Path $styleSource '*') $styleTarget -Recurse -Force
        Write-Output 'copied FluentWinUI3 style'
    } else {
        Write-Warning "FluentWinUI3 style not found at $styleSource"
    }

    # Keep opengl32sw.dll: without a GPU - a virtual machine, or a remote
    # desktop session - Qt has no other fallback and the window comes up black.
    $sw = Join-Path $OutDir 'opengl32sw.dll'
    if (Test-Path $sw) {
        Write-Output 'software OpenGL fallback present'
    } else {
        Write-Warning 'opengl32sw.dll missing; the app may not start over RDP'
    }

    $size = (Get-ChildItem $OutDir -Recurse -File |
             Measure-Object -Property Length -Sum).Sum / 1MB

    Write-Output ''
    Write-Output ("portable package ready: {0}  ({1:N1} MB)" -f $OutDir, $size)
    Write-Output 'copy the whole folder anywhere and run TMIXTOOL.exe'
}
finally {
    Pop-Location
}
