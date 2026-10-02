# One-time setup for building on Windows (x64, Visual Studio 2022 with the "Desktop development with C++" workload, CMake and git on
# the PATH):
#   1. fetches the submodules of the game (the code ones, and with -Data also the game's art, music and scripts, which are
#      large; a complete game needs them),
#   2. builds the game's dependencies with vcpkg into build\vcpkg_installed, using the game's own get-dependencies_win.ps1
#      (release build only, x64). Takes about 15 minutes the first time.
param(
	[switch]$Data = $true       # -Data:$false fetches only the code submodules (for DATA=none)
)
# Windows PowerShell 5.1 turns anything a native tool writes to stderr (git and vcpkg print progress there) into an error, so
# errors are detected through the exit code of each command instead.
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$build = Join-Path $root "build"
New-Item -ItemType Directory -Force $build | Out-Null

Write-Host "== submodules"
if ($Data) {
	& git -C $root submodule update --init --recursive --depth 1
} else {
	& git -C $root submodule update --init --recursive --depth 1 -- 3rdparty lib tools
}
if ($LASTEXITCODE -ne 0) { throw "git submodule update failed" }

Write-Host "== dependencies (vcpkg, x64-windows, release only)"
$env:VCPKG_DEFAULT_TRIPLET = "x64-windows"
$env:VCPKG_DEFAULT_HOST_TRIPLET = "x64-windows"
$env:VCPKG_DISABLE_METRICS = "1"
Remove-Item Env:VCPKG_VISUAL_STUDIO_PATH -ErrorAction SilentlyContinue
Push-Location $build
try {
	& (Join-Path $root "get-dependencies_win.ps1") release
	if ($LASTEXITCODE -ne 0) { throw "get-dependencies_win.ps1 failed" }
} finally {
	Pop-Location
}
Write-Host "Done. Next: make config build"
