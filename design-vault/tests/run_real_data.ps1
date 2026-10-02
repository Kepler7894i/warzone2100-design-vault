# Tries the game on COPIES of your own files: your stored designs and, optionally, a saved skirmish. Nothing in your own
# configuration folder is changed. Shows what the stored designs of a game made of those files look like after loading, and the
# file as it would be written back. Needs dist\ from design-vault\scripts\package.ps1.
param(
	[string]$UserConfig = "$env:APPDATA\Warzone 2100 Project\Warzone 2100",
	[string]$Save = ""        # for example  auto\Emergence_2026-04-30_221254  (a folder and a .gam in savegames\skirmish)
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$exe = Join-Path $root "dist\bin\warzone2100.exe"
$cfg = Join-Path $root "run\real-config"
$out = Join-Path $root "run\real-output"
foreach ($dir in @($cfg, $out)) {
	if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
	New-Item -ItemType Directory -Force $dir | Out-Null
}
New-Item -ItemType Directory -Force (Join-Path $cfg "userdata\mp"), (Join-Path $cfg "userdata\campaign"), (Join-Path $cfg "tests") | Out-Null
Copy-Item (Join-Path $UserConfig "userdata\mp\templates.json") (Join-Path $cfg "userdata\mp\templates.json")
Copy-Item (Join-Path $root "design-vault\tests\ui.json") (Join-Path $cfg "tests\ui.json")
$gameArgs = @("--skirmish=ui.json", "--autogame")
if ($Save) {
	$saves = Join-Path $UserConfig "savegames\skirmish"
	$target = Join-Path $cfg "savegames\skirmish"
	New-Item -ItemType Directory -Force (Split-Path (Join-Path $target $Save)) | Out-Null
	Copy-Item (Join-Path $saves $Save) (Join-Path $target $Save) -Recurse
	if (Test-Path (Join-Path $saves "$Save.gam")) { Copy-Item (Join-Path $saves "$Save.gam") (Join-Path $target "$Save.gam") }
	$gameArgs = @("--loadskirmish=$($Save.Replace('\','/'))")
}
$env:WZ_DESIGNVAULT_SELFTEST = "real"
$env:WZ_DESIGNVAULT_SELFTEST_OUT = Join-Path $out "real.txt"
$argList = @("--configdir=$cfg") + $gameArgs + @("--window", "--resolution=1024x768", "--nosound", "--noassert")
$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Minimized -WorkingDirectory (Split-Path $exe)
if (-not $p.WaitForExit(240000)) { $p.Kill(); Write-Host "game did not finish within 4 minutes" }
Remove-Item Env:WZ_DESIGNVAULT_SELFTEST, Env:WZ_DESIGNVAULT_SELFTEST_OUT
Write-Host "exit code $($p.ExitCode)"
if (Test-Path (Join-Path $out "real.txt")) { Get-Content (Join-Path $out "real.txt") }
Write-Host "backup file: $(Test-Path (Join-Path $cfg 'userdata\mp\templates.json.bak'))"
