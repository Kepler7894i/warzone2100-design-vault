# Builds the mod from your saved designs and installs it for the Steam copy of Warzone 2100: it is copied to
# <config>\mods\<game version>\multiplay\designvault.wz. It is a "multiplay" mod, so it only applies to skirmish and multiplayer
# games started with --mod_mp=designvault.wz (play-steam.ps1 does that), and never to the campaign.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$ConfigDir = "$env:APPDATA\Warzone 2100 Project\Warzone 2100"
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $GameDir "bin\warzone2100.exe"
if (-not (Test-Path $exe)) { throw "No Warzone 2100 at $exe" }
$version = (Get-Item $exe).VersionInfo.ProductVersion
if (-not $version) { throw "Could not read the version of $exe" }

$built = Join-Path $here "out\designvault.wz"
& (Join-Path $here "build-mod.ps1") -GameDir $GameDir -ConfigDir $ConfigDir -OutFile $built
$target = Join-Path $ConfigDir "mods\$version\multiplay"
New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item $built (Join-Path $target "designvault.wz") -Force
Write-Host "Installed $(Join-Path $target 'designvault.wz')"
