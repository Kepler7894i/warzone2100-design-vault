# Checks that the upgrade lines sit on top of the game's own saving, in both directions, on a COPY of a stored designs file (a fixed example, or one you pass with -Source):
#   1. this build loads the file the unmodified game wrote (no ids, no lines), makes an upgrade of the first design, and writes
#      the file (the same templates.json, the same format, with extra keys);
#   2. the unmodified Steam executable then loads that file, and every design of it must be in the game as before, plus the upgrade.
# Nothing in your own configuration folder is changed. Needs dist\ from "make package".
param(
	[string]$Source = "",   # a stored designs file as the unmodified game writes it; default: design-vault\tests\fixtures\stock-templates.json
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$mine = Join-Path $root "dist\bin\warzone2100.exe"
$stock = Join-Path $GameDir "bin\warzone2100.exe"
$cfg = Join-Path $root "run\compat-config"
$out = Join-Path $root "run\compat-output"
foreach ($dir in @($cfg, $out)) {
	if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
	New-Item -ItemType Directory -Force $dir | Out-Null
}
New-Item -ItemType Directory -Force "$cfg\userdata\mp", "$cfg\tests" | Out-Null
Copy-Item (Join-Path $root "design-vault\tests\ui.json") "$cfg\tests\ui.json"
if (-not $Source) { $Source = Join-Path $root "design-vault\tests\fixtures\stock-templates.json" }
Copy-Item $Source "$cfg\userdata\mp\templates.json"
$file = "$cfg\userdata\mp\templates.json"
$original = Get-Content $file -Raw | ConvertFrom-Json
$originalNames = @($original.templates | ForEach-Object { $_.name })
Write-Host "the file the unmodified game wrote: $(@($original.templates).Count) designs: $($originalNames -join ', ')"
Write-Host "it has vault keys: $((Get-Content $file -Raw) -match 'vaultId|supersedes')"

function Run-Game($exe, $extra, $seconds) {
	$argList = @("--configdir=$cfg", "--skirmish=ui.json", "--autogame") + $extra + @("--window", "--resolution=1024x768", "--nosound", "--noassert")
	$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Minimized -WorkingDirectory (Split-Path $exe)
	if (-not $p.WaitForExit($seconds * 1000)) { $p.Kill(); $p.WaitForExit(); Write-Host "game did not finish within $seconds s" }
	return $p.ExitCode
}

Write-Host "`n== 1. this build, on that file =="
$env:WZ_DESIGNVAULT_SELFTEST = "stockfile"
$env:WZ_DESIGNVAULT_SELFTEST_OUT = Join-Path $out "stockfile.txt"
$code = Run-Game $mine @() 240
Remove-Item Env:WZ_DESIGNVAULT_SELFTEST, Env:WZ_DESIGNVAULT_SELFTEST_OUT
Write-Host "exit code $code"
$report = Join-Path $out "stockfile.txt"
if (Test-Path $report) { Get-Content $report | Where-Object { $_ -match '^(ok|FAIL|design|[0-9]+ checks)' } }
$after = Get-Content $file -Raw | ConvertFrom-Json
Write-Host "the file now: $(@($after.templates).Count) designs, $(@($after.templates | Where-Object { $_.supersedes }).Count) in a line"
Write-Host "backup of the stock file kept: $(Test-Path "$file.bak")"

Write-Host "`n== 2. the unmodified Steam exe, on the file this build wrote =="
$code = Run-Game $stock @("--saveandquit=savegames/skirmish/compat.gam") 90
Write-Host "exit code $code"
$save = Get-ChildItem "$cfg\savegames" -Recurse -Filter "templates.json" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $save) { Write-Host "FAIL  the unmodified game did not get as far as saving"; exit 1 }
$t = Get-Content $save.FullName -Raw | ConvertFrom-Json
$names = @($t.localTemplates | ForEach-Object { $_.name })
$missing = @($after.templates | Where-Object { $names -notcontains $_.name } | ForEach-Object { $_.name })
Write-Host "stored designs in the file: $(@($after.templates).Count); of them missing in the unmodified game: $($missing.Count) $($missing -join ', ')"
if ($missing.Count -eq 0) { Write-Host "ok    the unmodified game reads the file this build wrote, every design is in its game" } else { Write-Host "FAIL  designs are missing" }
$errors = @(Get-ChildItem "$cfg\logs" -File -ErrorAction SilentlyContinue | Select-String -Pattern "template" | Where-Object { $_.Line -match "error|fail|invalid" })
Write-Host "template-related errors in the unmodified game's log: $($errors.Count)"
$errors | Select-Object -First 5 | ForEach-Object { Write-Host "  $($_.Line)" }
