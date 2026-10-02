# One-time setup for building on Windows (x64, Visual Studio 2022 with the "Desktop development with C++" workload, CMake and git on the PATH):
#   1. fetches the code submodules of the game (3rdparty\*), not the large data ones,
#   2. builds the game's dependencies with vcpkg into build\vcpkg_installed.
# Why not the game's own get-dependencies_win.ps1: the vcpkg it pins (2023) does not know the toolset of a current
# Visual Studio, and a current vcpkg does not work with the 2023 package set. design-vault\deps\vcpkg.json pins a 2025 set
# (the one release 4.6.3 uses, with SDL2) and design-vault\deps\.ci holds the overlay ports of that release.
# Takes a while (about 30 minutes the first time); the binary cache in build\vcpkg_cache makes a repeat quick.
param(
	[string]$VcpkgCommit = "1d483bc44f8c35627407d7024e0045743829c2e8"
)
# Windows PowerShell 5.1 turns anything a native tool writes to stderr (git and vcpkg print progress there) into an error, so
# errors are detected through the exit code of each command instead.
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$build = Join-Path $root "build"
New-Item -ItemType Directory -Force $build | Out-Null

Write-Host "== code submodules"
& git -C $root submodule update --init --depth 1 -- 3rdparty lib/sound/3rdparty/opusfile
if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }

Write-Host "== vcpkg"
$vcpkgRoot = Join-Path $build "vcpkg"
if (-not (Test-Path (Join-Path $vcpkgRoot ".git"))) {
	& git clone --filter=blob:none https://github.com/microsoft/vcpkg.git $vcpkgRoot
	if ($LASTEXITCODE -ne 0) { throw "git clone vcpkg failed" }
}
& git -C $vcpkgRoot checkout $VcpkgCommit
if ($LASTEXITCODE -ne 0) { throw "could not check out vcpkg $VcpkgCommit" }
if (-not (Test-Path (Join-Path $vcpkgRoot "vcpkg.exe"))) {
	& (Join-Path $vcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
	if ($LASTEXITCODE -ne 0) { throw "bootstrap-vcpkg failed" }
}

Write-Host "== dependencies (x64-windows, release only)"
$env:VCPKG_DEFAULT_TRIPLET = "x64-windows"
$env:VCPKG_DEFAULT_HOST_TRIPLET = "x64-windows"
$env:VCPKG_OVERLAY_TRIPLETS = Join-Path $root "design-vault\deps\triplets"
$env:VCPKG_BINARY_SOURCES = "clear;files,$(Join-Path $build 'vcpkg_cache'),readwrite"
$env:VCPKG_DISABLE_METRICS = "1"
Remove-Item Env:VCPKG_VISUAL_STUDIO_PATH -ErrorAction SilentlyContinue
& (Join-Path $vcpkgRoot "vcpkg.exe") install `
	"--vcpkg-root=$vcpkgRoot" `
	"--x-manifest-root=$(Join-Path $root 'design-vault\deps')" `
	"--x-install-root=$(Join-Path $build 'vcpkg_installed')" `
	"--overlay-ports=$(Join-Path $root 'design-vault\deps\.ci\vcpkg\overlay-ports')"
if ($LASTEXITCODE -ne 0) { throw "vcpkg install failed" }
Write-Host "Done. Next: make config build"
