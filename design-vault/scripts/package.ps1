# Assembles a runnable copy of the game in dist\ : the freshly built warzone2100.exe and its DLLs, next to the
# game data of an installed copy of Warzone 2100 (the Steam install by default). Nothing in the installed copy is changed:
# the data folder is only linked, so it is never duplicated and always matches what the installed game has.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$Config = "Release"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$build = Join-Path $root "build\wz"
$exe = @("src\warzone2100.exe", "src\$Config\warzone2100.exe") | ForEach-Object { Join-Path $build $_ } | Where-Object { Test-Path $_ } | Select-Object -First 1
$dlls = Join-Path $root "build\vcpkg_installed\x64-windows\bin"
$dist = Join-Path $root "dist"

if (-not $exe) { throw "Build the game first: no warzone2100.exe under $build" }
if (-not (Test-Path (Join-Path $GameDir "data\base.wz"))) { throw "No Warzone 2100 data found in $GameDir" }

$bin = Join-Path $dist "bin"
New-Item -ItemType Directory -Force $bin | Out-Null
Copy-Item $exe $bin -Force
Get-ChildItem $dlls -Filter *.dll | Copy-Item -Destination $bin -Force

foreach ($name in "data", "locale") {
	$link = Join-Path $dist $name
	$target = Join-Path $GameDir $name
	if (-not (Test-Path $target)) { continue }
	if (Test-Path $link) { (Get-Item $link).Delete() }   # removes a junction without touching what it points to
	New-Item -ItemType Junction -Path $link -Target $target | Out-Null
}
Write-Host "Ready: $bin\warzone2100.exe"
