# Removes a saved design for good: from the mod's library, from the lines that mention it, and from the game's own stored
# designs file, so that the next sync does not bring it back. Then rebuilds and installs the mod.
# The game must not be running (it writes its stored designs file when it quits). Backups: designs.json.bak and templates.json.bak.
#
#   .\remove-design.ps1 -Name "Light Cannon Cobra Hover"        # by the name it has in the game (all designs with that name)
#   .\remove-design.ps1 -Name DV_0004                           # or by id
param(
	[Parameter(Mandatory = $true)][string]$Name,
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$ConfigDir = "$env:APPDATA\Warzone 2100 Project\Warzone 2100"
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if ($ConfigDir -eq "$env:APPDATA\Warzone 2100 Project\Warzone 2100" -and (Get-Process warzone2100 -ErrorAction SilentlyContinue)) { throw "Quit Warzone 2100 first: it writes its stored designs when it quits, which would undo this." }
$utf8 = [System.Text.UTF8Encoding]::new($false)
$libraryFile = Join-Path $ConfigDir "userdata\designvault\designs.json"
$linesFile = Join-Path $ConfigDir "userdata\designvault\lines.txt"
$storedFile = Join-Path $ConfigDir "userdata\mp\templates.json"
if (-not (Test-Path $libraryFile)) { throw "There is no library yet ($libraryFile). Run install.ps1 first." }

$library = Get-Content $libraryFile -Raw | ConvertFrom-Json
$match = @($library.designs | Where-Object { $_.id -ieq $Name -or $_.name -ieq $Name })
if ($match.Count -eq 0) { throw "No saved design is called '$Name'" }
$keep = @($library.designs | Where-Object { $match -notcontains $_ })

function Get-Part($design, $field) { if ($design.PSObject.Properties[$field]) { return [string]$design.$field } else { return "" } }
function Get-Weapons($design) { if ($design.PSObject.Properties["weapons"]) { return @($design.weapons | ForEach-Object { [string]$_ }) } else { return @() } }
function Get-DesignKey($design) {
	$parts = @("type", "body", "propulsion", "brain", "repair", "ecm", "sensor", "construct") | ForEach-Object { Get-Part $design $_ }
	return ($parts -join "|") + "|" + ((Get-Weapons $design) -join ",")
}
$removedKeys = @{}
foreach ($m in $match) { $removedKeys[(Get-DesignKey $m)] = $true }

Copy-Item $libraryFile "$libraryFile.bak" -Force
$library.designs = $keep
[System.IO.File]::WriteAllText($libraryFile, ($library | ConvertTo-Json -Depth 6), $utf8)
Write-Host "Removed from the library: $(($match | ForEach-Object { "$($_.name) ($($_.id))" }) -join ', ')"

if (Test-Path $storedFile) {
	$stored = Get-Content $storedFile -Raw | ConvertFrom-Json
	$left = @($stored.templates | Where-Object { -not $removedKeys.ContainsKey((Get-DesignKey $_)) })
	$dropped = @($stored.templates).Count - $left.Count
	if ($dropped -gt 0) {
		Copy-Item $storedFile "$storedFile.bak" -Force
		$stored.templates = $left
		[System.IO.File]::WriteAllText($storedFile, ($stored | ConvertTo-Json -Depth 6), $utf8)
		Write-Host "Removed $dropped entr$(if ($dropped -eq 1) { 'y' } else { 'ies' }) from the game's stored designs ($storedFile)"
	}
}

if (Test-Path $linesFile) {
	$ids = @($match | ForEach-Object { $_.id.ToLowerInvariant() })
	$names = @($match | ForEach-Object { ([string]$_.name).ToLowerInvariant() })
	$kept = foreach ($line in Get-Content $linesFile) {
		if ($line -match '^\s*(.+?)\s+replaces\s+(.+?)\s*$' -and (($ids + $names) -contains $Matches[1].ToLowerInvariant() -or ($ids + $names) -contains $Matches[2].ToLowerInvariant())) {
			Write-Host "Removed the line: $line"
		} else { $line }
	}
	[System.IO.File]::WriteAllLines($linesFile, [string[]]@($kept), $utf8)
}
& (Join-Path $here "install.ps1") -GameDir $GameDir -ConfigDir $ConfigDir
