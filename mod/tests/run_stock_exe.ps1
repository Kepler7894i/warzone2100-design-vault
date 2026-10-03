# Runs the UNMODIFIED Steam executable with the mod, in a scripted skirmish with its own configuration folder, and shows what the
# mod's script logged. Your real settings and saves are not touched. Run mod\build-mod.ps1 first, or let this script do it
# from a copy of your stored designs.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[int]$Seconds = 45,
	[int]$TechLevel = 3      # 1: very little is researched at the start, 3: most things are
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$exe = Join-Path $GameDir "bin\warzone2100.exe"
$cfg = Join-Path $root "run\stock-config"
$version = (Get-Item $exe).VersionInfo.ProductVersion
$modsFolder = if ($version -match '^\d+\.\d+\.\d+$') { "mods\$version\multiplay" } else { "mods\multiplay" }   # see install.ps1
if (Test-Path $cfg) { Remove-Item $cfg -Recurse -Force }
New-Item -ItemType Directory -Force "$cfg\tests", "$cfg\userdata\mp", "$cfg\userdata\designvault", (Join-Path $cfg $modsFolder) | Out-Null
(Get-Content (Join-Path $root "mod\tests\ui.json") -Raw).Replace('"techLevel": 3', '"techLevel": ' + $TechLevel) | Set-Content -Encoding UTF8 "$cfg\tests\ui.json"
Copy-Item (Join-Path $root "mod\tests\stock-templates.json") "$cfg\userdata\mp\templates.json"   # a fixed set of stored designs, not yours
@"
Light Cannon Cobra Hover replaces Twin Machinegun Cobra Hover
Mini-Rocket Array Cobra Hover replaces Light Cannon Cobra Hover
"@ | Set-Content -Encoding UTF8 "$cfg\userdata\designvault\lines.txt"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root "mod\build-mod.ps1") -GameDir $GameDir -ConfigDir $cfg -OutFile (Join-Path $cfg "$modsFolder\designvault.wz") | Out-Host

# --saveandquit saves the game as soon as it has started (after the rules, and with them the mod, have run) and quits. The saved
# game contains the lists of designs exactly as the game had them, which is what the design screen and the factories show.
$argList = @("--configdir=$cfg", "--mod_mp=designvault.wz", "--skirmish=ui.json", "--autogame", "--saveandquit=savegames/skirmish/vtmod.gam", "--window", "--resolution=1024x768", "--nosound", "--noassert")
$p = Start-Process -FilePath $exe -ArgumentList $argList -PassThru -WindowStyle Minimized -WorkingDirectory (Split-Path $exe)
if (-not $p.WaitForExit($Seconds * 1000)) { $p.Kill(); $p.WaitForExit(); Write-Host "game did not quit by itself within $Seconds s" }
Write-Host "game exit code: $($p.ExitCode)"
foreach ($f in Get-ChildItem "$cfg\logs" -File) {
	$hits = Select-String -Path $f.FullName -Pattern "design vault|multiplay mod" -ErrorAction SilentlyContinue
	foreach ($h in $hits) { Write-Host ("{0}: {1}" -f $f.Name, $h.Line) }
}
$save = Get-ChildItem "$cfg\savegames" -Recurse -Filter "templates.json" | Select-Object -First 1
if (-not $save) { Write-Host "no saved game was written"; exit 1 }
$t = Get-Content $save.FullName -Raw | ConvertFrom-Json
Write-Host ""
Write-Host "Designs in the list the game builds the design screen and factory lists from (localTemplates):"
foreach ($l in $t.localTemplates) { if ($l.name -match "Cobra Hover") { Write-Host ("  {0}  [{1}] enabled={2}" -f $l.name, $l.ref, $l.enabled) } }
$count = @($t.localTemplates).Count
Write-Host "($count designs in total)"
