# End-to-end self-test of the stored designs, in the real game.
#
# Every scenario starts the game in its own configuration folder (the real settings, saves and stored designs are never touched),
# runs a check from inside the game (src\designvault_selftest.cpp) and quits with exit code 0 when everything held.
#
#   skirmish line   "build" creates a line of designs through the same code the design screen uses, checks what the factory and the
#                   design screen offer as research changes, deletes a design from the middle of the line and checks the file.
#                   Then a design from an older version of the file (no identity) and a design with a component that does not
#                   exist are added by hand, and "check" runs in a fresh process: everything must have come back from the file,
#                   the unloadable design must have been kept, and a backup of the previous file must exist.
#   saved game      "save" stores designs, saves the game and deletes one stored design afterwards; "load" loads that saved game in a
#                   fresh process: no duplicates, the line is intact, and the deleted design is not stored again.
#   campaign        one campaign game stores a design, the next one finds it.
#   design screen   "ui" presses the buttons of the design screen (Upgrade, the disk, the bin, show obsolete) and takes screenshots.
#
# Needs dist\ from "make package". Run from anywhere:  powershell -File design-vault\tests\run_selftest.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$exe = Join-Path $root "dist\bin\warzone2100.exe"
$runRoot = Join-Path $root "run"
if (-not (Test-Path $exe)) { throw "Run make package first" }

function Reset-Folder($path) {
	if (Test-Path $path) {
		if (-not ((Resolve-Path $path).Path.StartsWith($runRoot))) { throw "Refusing to delete outside the run folder: $path" }
		Remove-Item $path -Recurse -Force
	}
	New-Item -ItemType Directory -Force $path | Out-Null
}

$skirmish = @'
{
	"challenge": {
		"bases": 2,
		"difficulty": "Easy",
		"map": "Sk-Rush",
		"maxPlayers": 4,
		"powerLevel": 2,
		"scavengers": "false",
		"techLevel": 3,
		"version": 2
	},
	"player_0": { "team": 0 },
	"player_1": { "difficulty": "Easy", "ai": "multiplay/skirmish/semperfi.js", "name": "Ally", "team": 0 },
	"player_2": { "difficulty": "Easy", "ai": "multiplay/skirmish/semperfi.js", "name": "Enemy", "team": 1 },
	"player_3": { "difficulty": "Easy", "ai": "multiplay/skirmish/semperfi.js", "name": "Enemy", "team": 1 }
}
'@

function New-Config($name) {
	$cfg = Join-Path $runRoot $name
	Reset-Folder $cfg
	New-Item -ItemType Directory -Force (Join-Path $cfg "tests") | Out-Null
	$skirmish | Set-Content -Encoding UTF8 (Join-Path $cfg "tests\selftest.json")
	return $cfg
}

# Runs one scenario and returns 0 when it passed.
function Invoke-Game($cfg, $mode, $gameArgs) {
	$out = Join-Path $runRoot "selftest-output"
	New-Item -ItemType Directory -Force $out | Out-Null
	$report = Join-Path $out "$($mode).txt"
	if (Test-Path $report) { Remove-Item $report }
	$env:WZ_DESIGNVAULT_SELFTEST = $mode
	$env:WZ_DESIGNVAULT_SELFTEST_OUT = $report
	$argList = @("--configdir=$cfg") + $gameArgs + @("--window", "--resolution=1024x768", "--nosound", "--noassert")
	$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Minimized -WorkingDirectory (Split-Path $exe)
	if (-not $p.WaitForExit(240000)) {
		$p.Kill()
		Remove-Item Env:WZ_DESIGNVAULT_SELFTEST, Env:WZ_DESIGNVAULT_SELFTEST_OUT
		Write-Host "[$mode] did not finish within 4 minutes"
		return 1
	}
	Remove-Item Env:WZ_DESIGNVAULT_SELFTEST, Env:WZ_DESIGNVAULT_SELFTEST_OUT
	$code = $p.ExitCode
	Write-Host "[$mode] exit code $code"
	if (Test-Path $report) { Get-Content $report | Where-Object { $_ -match "^(ok|FAIL|design vault|[0-9]+ checks)" } | Out-Host }
	else { Write-Host "no report was written"; return 1 }
	return $code
}

$failed = 0
$skirmishArgs = @("--skirmish=selftest.json", "--autogame")

# --- skirmish line, then a fresh process
$cfg = New-Config "selftest-config"
$failed += [int](Invoke-Game $cfg "build" $skirmishArgs)
$vault = Join-Path $cfg "userdata\mp\templates.json"
if (-not (Test-Path $vault)) { throw "the build run did not write $vault" }
$json = Get-Content $vault -Raw | ConvertFrom-Json
$entries = @($json.templates)
$entries += [pscustomobject]@{ name = "VT Legacy"; type = "WEAPON"; body = "Body1REC"; propulsion = "wheeled01"; weapons = @("MG1Mk1") }
$entries += [pscustomobject]@{ name = "VT Ghost"; type = "WEAPON"; body = "BodyThatDoesNotExist"; propulsion = "wheeled01"; weapons = @("MG1Mk1"); vaultId = "ffffffffffffffff" }
$json.templates = $entries
$json | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $vault
$failed += [int](Invoke-Game $cfg "check" $skirmishArgs)
if (Test-Path "$vault.bak") { Write-Host "backup of the previous file was kept: yes" } else { Write-Host "backup of the previous file was kept: NO"; $failed += 1 }

# --- saved game
$cfg = New-Config "selftest-config-save"
$failed += [int](Invoke-Game $cfg "save" $skirmishArgs)
$failed += [int](Invoke-Game $cfg "load" @("--loadskirmish=vttest"))

# --- campaign
$cfg = New-Config "selftest-config-campaign"
$failed += [int](Invoke-Game $cfg "campaign-build" @("--game=CAM_1A"))
$failed += [int](Invoke-Game $cfg "campaign-check" @("--game=CAM_1A"))

if ($failed -eq 0) { Write-Host "SELF-TEST PASSED" } else { Write-Host "SELF-TEST FAILED"; exit 1 }
