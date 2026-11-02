param(
    [string]$QtRoot = $env:QT_ROOT_PATH,
    [string]$ConanHome = $env:CONAN_HOME,
    [switch]$Tests
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
Push-Location $repo

try {
    $pythonDirs = Get-ChildItem (Join-Path $env:APPDATA "Python") -Directory -Filter 'Python*' -ErrorAction SilentlyContinue
    foreach ($pythonDir in $pythonDirs) {
        $scripts = Join-Path $pythonDir.FullName "Scripts"
        if (Test-Path $scripts) {
            $env:PATH = "$scripts;$env:PATH"
        }
    }

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw "cmake not found. Install it with: py -m pip install --user cmake==3.31.10"
    }

    if (-not (Get-Command conan -ErrorAction SilentlyContinue)) {
        throw "conan not found. Install it with: py -m pip install --user conan==2.28.0"
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "Visual Studio Build Tools not found. Install the C++ workload first, see BUILD_WINDOWS.md"
    }
    $vsPath = & $vswhere -latest -products * -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        throw "MSVC 2022 C++ tools not found. Install Desktop development with C++, see BUILD_WINDOWS.md"
    }
    $env:VCVARS_PATH = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"

    if ($QtRoot) {
        $qtPlatform = Join-Path $QtRoot "msvc2022_64"
        if (-not (Test-Path (Join-Path $qtPlatform "bin\qmake.exe"))) {
            throw "Qt not found at $qtPlatform. Install Qt 6.10.1 with BUILD_WINDOWS.md"
        }
        $env:QT_ROOT_PATH = $QtRoot
    }

    if ($ConanHome) {
        New-Item -ItemType Directory -Force -Path $ConanHome | Out-Null
        $env:CONAN_HOME = $ConanHome
    }

    if ($Tests) {
        $env:AMNEZIA_BUILD_TESTS = "ON"
    } elseif (-not $env:AMNEZIA_BUILD_TESTS) {
        $env:AMNEZIA_BUILD_TESTS = if ($Tests) { "ON" } else { "OFF" }
    }

    Write-Host "Qt:        $(if ($QtRoot) { $QtRoot } else { 'auto-detect' })"
    Write-Host "Repo:      $repo"
    Write-Host "Build:     deploy\build"
    Write-Host "Conan home:$(if ($ConanHome) { $ConanHome } else { 'default' })"
    Write-Host ""

    cmd /c "deploy\build.bat"
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    Write-Host ""
    Write-Host "Build finished. Executable:"
    Get-ChildItem -Path "deploy\build" -Recurse -Filter "*.exe" |
        Where-Object { $_.Name -notmatch "service" } |
        Select-Object -First 10 FullName
}
finally {
    Pop-Location
}
