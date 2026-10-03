# Builds the mod from your saved designs and installs it for the Steam copy of Warzone 2100: it is copied to
# <config>\mods\<game version>\multiplay\designvault.wz (mods\multiplay for a development build). It is a "multiplay" mod, so it only applies to skirmish and multiplayer
# games started with --mod_mp=designvault.wz (play-steam.ps1 does that), and never to the campaign.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$ConfigDir = "$env:APPDATA\Warzone 2100 Project\Warzone 2100"
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $GameDir "bin\warzone2100.exe"
if (-not (Test-Path $exe)) { throw "No Warzone 2100 at $exe" }
# The game looks for mods in mods\<version>\multiplay when it is a release (its version is like 4.7.0), and in mods\multiplay
# for a build that is not exactly on a release tag (a development build).
function Get-ModsFolder($exe) {
	$version = (Get-Item $exe).VersionInfo.ProductVersion
	if ($version -match '^\d+\.\d+\.\d+$') { return "mods\$version\multiplay" } else { return "mods\multiplay" }
}

$built = Join-Path $here "out\designvault.wz"
& (Join-Path $here "build-mod.ps1") -GameDir $GameDir -ConfigDir $ConfigDir -OutFile $built
$target = Join-Path $ConfigDir (Get-ModsFolder $exe)
New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item $built (Join-Path $target "designvault.wz") -Force
Write-Host "Installed $(Join-Path $target 'designvault.wz')"
