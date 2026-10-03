# Removes the mod from the Steam copy of Warzone 2100. Your saved designs (the library) and your lines are kept, in
# <config>\userdata\designvault.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$ConfigDir = "$env:APPDATA\Warzone 2100 Project\Warzone 2100"
)
$ErrorActionPreference = "Stop"
$exe = Join-Path $GameDir "bin\warzone2100.exe"
# The game looks for mods in mods\<version>\multiplay when it is a release (its version is like 4.7.0), and in mods\multiplay
# for a build that is not exactly on a release tag (a development build).
function Get-ModsFolder($exe) {
	$version = (Get-Item $exe).VersionInfo.ProductVersion
	if ($version -match '^\d+\.\d+\.\d+$') { return "mods\$version\multiplay" } else { return "mods\multiplay" }
}
$file = Join-Path $ConfigDir (Join-Path (Get-ModsFolder $exe) "designvault.wz")
if (Test-Path $file) { Remove-Item $file; Write-Host "Removed $file" } else { Write-Host "The mod is not installed ($file)" }
