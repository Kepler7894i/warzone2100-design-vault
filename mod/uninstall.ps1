# Removes the mod from the Steam copy of Warzone 2100. Your saved designs (the library) and your lines are kept, in
# <config>\userdata\designvault.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$ConfigDir = "$env:APPDATA\Warzone 2100 Project\Warzone 2100"
)
$ErrorActionPreference = "Stop"
$version = (Get-Item (Join-Path $GameDir "bin\warzone2100.exe")).VersionInfo.ProductVersion
$file = Join-Path $ConfigDir "mods\$version\multiplay\designvault.wz"
if (Test-Path $file) { Remove-Item $file; Write-Host "Removed $file" } else { Write-Host "The mod is not installed ($file)" }
