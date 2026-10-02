# Drives the design screen of the real game through the handlers of its buttons (Upgrade, the disk, the bin, show obsolete) and
# takes screenshots of it. Own configuration folder, your real settings are not touched. A game window is visible for about a
# minute. Screenshots end up in run\ui-config\screenshots. Needs dist\ from "make package".
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$exe = Join-Path $root "dist\bin\warzone2100.exe"
$cfg = Join-Path $root "run\ui-config"
$out = Join-Path $root "run\ui-output"
foreach ($path in $cfg, $out) {
	if (Test-Path $path) { Remove-Item $path -Recurse -Force }
	New-Item -ItemType Directory -Force $path | Out-Null
}
New-Item -ItemType Directory -Force (Join-Path $cfg "tests") | Out-Null
@'
{
	"challenge": { "bases": 2, "difficulty": "Easy", "map": "Sk-Rush", "maxPlayers": 4, "powerLevel": 2, "scavengers": "false", "techLevel": 3, "version": 2 },
	"player_0": { "team": 0 },
	"player_1": { "difficulty": "Easy", "ai": "multiplay/skirmish/semperfi.js", "name": "Ally", "team": 0 },
	"player_2": { "difficulty": "Easy", "ai": "multiplay/skirmish/semperfi.js", "name": "Enemy", "team": 1 },
	"player_3": { "difficulty": "Easy", "ai": "multiplay/skirmish/semperfi.js", "name": "Enemy", "team": 1 }
}
'@ | Set-Content -Encoding UTF8 (Join-Path $cfg "tests\ui.json")

$env:WZ_DESIGNVAULT_SELFTEST = "ui"
$env:WZ_DESIGNVAULT_SELFTEST_OUT = Join-Path $out "ui.txt"
$args = @("--configdir=$cfg", "--skirmish=ui.json", "--autogame", "--window", "--resolution=1280x800", "--nosound", "--noassert")
$p = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WorkingDirectory (Split-Path $exe)
if (-not $p.WaitForExit(240000)) { $p.Kill(); Write-Host "game did not finish within 4 minutes" }
Remove-Item Env:WZ_DESIGNVAULT_SELFTEST, Env:WZ_DESIGNVAULT_SELFTEST_OUT
Write-Host "exit code $($p.ExitCode)"
if (Test-Path (Join-Path $out "ui.txt")) { Get-Content (Join-Path $out "ui.txt") }
Get-ChildItem (Join-Path $cfg "screenshots") -ErrorAction SilentlyContinue | ForEach-Object { Write-Host $_.FullName }
