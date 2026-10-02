# Finds out which designs end up in the design screen and the factory lists of a game with AI allies and enemies.
# Runs a scripted skirmish (own configuration folder, your real settings are not touched) at 10x speed for a few minutes of game time,
# and writes every design the game knows about at a few moments. Needs dist\ from design-vault\scripts\package.ps1.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$AllyAi = "multiplay/skirmish/nexus.js"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$exe = Join-Path $root "dist\bin\warzone2100.exe"
$cfg = Join-Path $root "run\dump-config"
$out = Join-Path $root "run\dump-output"
foreach ($path in $cfg, $out) {
	if (Test-Path $path) { Remove-Item $path -Recurse -Force }
	New-Item -ItemType Directory -Force $path | Out-Null
}
New-Item -ItemType Directory -Force (Join-Path $cfg "tests") | Out-Null
@"
{
	"challenge": { "bases": 1, "difficulty": "Medium", "map": "Sk-Rush", "maxPlayers": 4, "powerLevel": 2, "scavengers": "false", "techLevel": 1, "version": 2 },
	"player_0": { "team": 0 },
	"player_1": { "difficulty": "Medium", "ai": "$AllyAi", "name": "Ally", "team": 0 },
	"player_2": { "difficulty": "Medium", "ai": "$AllyAi", "name": "Enemy", "team": 1 },
	"player_3": { "difficulty": "Medium", "ai": "$AllyAi", "name": "Enemy", "team": 1 }
}
"@ | Set-Content -Encoding UTF8 (Join-Path $cfg "tests\dump.json")

$env:WZ_DESIGNVAULT_SELFTEST = "dump"
$env:WZ_DESIGNVAULT_SELFTEST_OUT = Join-Path $out "dump.txt"
$args = @("--configdir=$cfg", "--skirmish=dump.json", "--autogame", "--window", "--resolution=1024x768", "--nosound", "--noassert")
$p = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WindowStyle Minimized -WorkingDirectory (Split-Path $exe)
if (-not $p.WaitForExit(900000)) { $p.Kill(); Write-Host "stopped after 15 minutes" }
Remove-Item Env:WZ_DESIGNVAULT_SELFTEST, Env:WZ_DESIGNVAULT_SELFTEST_OUT
Write-Host "exit code $($p.ExitCode); report: $env:WZ_DESIGNVAULT_SELFTEST_OUT"
