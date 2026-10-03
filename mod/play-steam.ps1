# Starts the Steam copy of Warzone 2100 with the Design Vault mod, so that your saved designs and lines are in the game.
#
#   1. builds and installs the mod from your saved designs (picks up what you stored with the disk button last time),
#   2. starts the game with --mod_mp=designvault.wz,
#   3. when you quit the game, builds the mod again, so that designs stored in this session are in the library for next time.
#
# To start it from Steam itself instead, set the game's launch option (Steam > Warzone 2100 > Properties) to
#       --mod_mp=designvault.wz
# and run mod\install.ps1 after storing designs.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$ConfigDir = "$env:APPDATA\Warzone 2100 Project\Warzone 2100",
	[switch]$NoWait,      # start the game and return at once, without the second build
	[string[]]$GameArgs = @()
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path $GameDir "bin\warzone2100.exe"
$defaultConfig = "$env:APPDATA\Warzone 2100 Project\Warzone 2100"

& (Join-Path $here "install.ps1") -GameDir $GameDir -ConfigDir $ConfigDir
$argList = @("--mod_mp=designvault.wz") + $GameArgs
if ($ConfigDir -ne $defaultConfig) { $argList += "--configdir=$ConfigDir" }
$p = Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory (Split-Path $exe) -PassThru
Write-Host "Warzone 2100 started (process $($p.Id)) with the Design Vault mod."
if (-not $NoWait) {
	$p.WaitForExit()
	& (Join-Path $here "install.ps1") -GameDir $GameDir -ConfigDir $ConfigDir
}
