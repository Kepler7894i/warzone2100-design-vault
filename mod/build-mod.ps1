# Builds designvault.wz, a mod for the unmodified Warzone 2100 4.4.x (the Steam one) from your saved designs.
#
#   1. Collects designs into the mod's own library (<config>\userdata\designvault\designs.json): everything the game has
#      stored with its disk button (<config>\userdata\mp\templates.json), plus what the library already had. Nothing is ever
#      dropped from the library, and identical designs are merged.
#   2. Reads the lines (<config>\userdata\designvault\lines.txt, or mod\lines.txt as a starting point): "NEWER replaces OLDER".
#   3. Writes the mod: the game's own list of built-in designs plus the library as extra built-in designs that are offered
#      once researched (stats/templates.json), and a script that hides the older design of a line once the newer one can be
#      built (multiplay/script/mods/init.js).
#
# Run it again whenever you have stored new designs or edited the lines. play-steam.ps1 does that for you.
param(
	[string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Warzone 2100",
	[string]$ConfigDir = "$env:APPDATA\Warzone 2100 Project\Warzone 2100",
	[string]$OutFile = "",
	[switch]$NoLibraryUpdate
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutFile) { $OutFile = Join-Path $here "out\designvault.wz" }
$utf8 = [System.Text.UTF8Encoding]::new($false)

$mpWz = Join-Path $GameDir "data\mp.wz"
if (-not (Test-Path $mpWz)) { throw "No Warzone 2100 game data at $mpWz" }

function Read-ZipText($zip, $name) {
	$entry = $zip.GetEntry($name)
	if (-not $entry) { throw "$name is not in $mpWz" }
	$reader = [System.IO.StreamReader]::new($entry.Open(), $utf8)
	try { return $reader.ReadToEnd() } finally { $reader.Close() }
}

# --- what the game itself has: its built-in designs and the names of all components
$zip = [System.IO.Compression.ZipFile]::OpenRead($mpWz)
try {
	$stockTemplates = Read-ZipText $zip "stats/templates.json"
	$componentIds = @{}
	foreach ($file in "body", "propulsion", "weapons", "brain", "ecm", "sensor", "repair", "construction") {
		$stats = (Read-ZipText $zip "stats/$file.json") | ConvertFrom-Json
		foreach ($property in $stats.PSObject.Properties) { $componentIds[$property.Name] = $true }
	}
	$stockIds = @{}
	foreach ($property in ($stockTemplates | ConvertFrom-Json).PSObject.Properties) { $stockIds[$property.Name] = $true }
} finally { $zip.Dispose() }

# --- the library
$dataDir = Join-Path $ConfigDir "userdata\designvault"
$libraryFile = Join-Path $dataDir "designs.json"
$linesFile = Join-Path $dataDir "lines.txt"
$storedFile = Join-Path $ConfigDir "userdata\mp\templates.json"
New-Item -ItemType Directory -Force $dataDir | Out-Null
if (-not (Test-Path $linesFile)) {
	$starter = Join-Path $here "lines.txt"
	if (Test-Path $starter) { Copy-Item $starter $linesFile }
}

function Get-Part($design, $name) { if ($design.PSObject.Properties[$name]) { return [string]$design.$name } else { return "" } }
function Get-Weapons($design) { if ($design.PSObject.Properties["weapons"]) { return @($design.weapons | ForEach-Object { [string]$_ }) } else { return @() } }
function Get-DesignKey($design) {
	$parts = @("type", "body", "propulsion", "brain", "repair", "ecm", "sensor", "construct") | ForEach-Object { Get-Part $design $_ }
	return ($parts -join "|") + "|" + ((Get-Weapons $design) -join ",")
}

$library = [pscustomobject]@{ version = 1; nextId = 1; designs = @() }
if (Test-Path $libraryFile) { $library = Get-Content $libraryFile -Raw | ConvertFrom-Json }
$designs = @($library.designs)
$byKey = @{}
foreach ($d in $designs) { $byKey[(Get-DesignKey $d)] = $d }

$added = 0
if (-not $NoLibraryUpdate -and (Test-Path $storedFile)) {
	$stored = Get-Content $storedFile -Raw | ConvertFrom-Json
	foreach ($s in @($stored.templates)) {
		if ($s.PSObject.Properties["name"] -eq $null) { continue }
		$key = Get-DesignKey $s
		if ($byKey.ContainsKey($key)) { continue }
		$entry = [ordered]@{ id = ("DV_{0:D4}" -f [int]$library.nextId); name = [string]$s.name }
		foreach ($field in "type", "body", "propulsion", "brain", "repair", "ecm", "sensor", "construct") {
			$value = Get-Part $s $field
			if ($value) { $entry[$field] = $value }
		}
		$weapons = @(Get-Weapons $s)
		if ($weapons.Count -gt 0) { $entry["weapons"] = $weapons }
		$library.nextId = [int]$library.nextId + 1
		$designs += [pscustomobject]$entry
		$byKey[$key] = $designs[-1]
		$added++
	}
	if ($added -gt 0) {
		if (Test-Path $libraryFile) { Copy-Item $libraryFile "$libraryFile.bak" -Force }
		$library.designs = $designs
		[System.IO.File]::WriteAllText($libraryFile, ($library | ConvertTo-Json -Depth 6), $utf8)
	}
}
Write-Host "Library: $($designs.Count) designs ($added new from $storedFile)"

# --- designs the mod can use
$usable = @()
foreach ($d in $designs) {
	$parts = @("body", "propulsion", "brain", "repair", "ecm", "sensor", "construct") | ForEach-Object { Get-Part $d $_ } | Where-Object { $_ }
	$parts = @($parts) + @(Get-Weapons $d)
	$unknown = @($parts | Where-Object { -not $componentIds.ContainsKey($_) })
	if ($unknown.Count -gt 0) { Write-Warning "Left out '$($d.name)' ($($d.id)): this game has no component $($unknown -join ', ')"; continue }
	if ($stockIds.ContainsKey($d.id)) { Write-Warning "Left out '$($d.name)': the id $($d.id) is used by the game itself"; continue }
	$usable += [pscustomobject]@{ design = $d; parts = $parts }
}

# --- the lines
$replaces = @{}
$byName = @{}
$byId = @{}
foreach ($u in $usable) {
	$byId[$u.design.id.ToLowerInvariant()] = $u.design
	$n = ([string]$u.design.name).ToLowerInvariant()
	if (-not $byName.ContainsKey($n)) { $byName[$n] = @() }
	$byName[$n] += $u.design
}
function Find-Design($text) {
	$t = $text.Trim().ToLowerInvariant()
	if ($byId.ContainsKey($t)) { return $byId[$t] }
	if ($byName.ContainsKey($t)) {
		if ($byName[$t].Count -gt 1) { Write-Warning "Several designs are called '$text', use one of their ids instead: $(($byName[$t] | ForEach-Object { $_.id }) -join ', ')"; return $null }
		return $byName[$t][0]
	}
	Write-Warning "Unknown design '$text'"
	return $null
}
$lineCount = 0
if (Test-Path $linesFile) {
	$number = 0
	foreach ($line in Get-Content $linesFile) {
		++$number
		$line = $line.Trim()
		if (-not $line -or $line.StartsWith("#")) { continue }
		if ($line -notmatch '^(.+?)\s+replaces\s+(.+)$') { Write-Warning "lines.txt line ${number}: expected 'NEWER replaces OLDER'"; continue }
		$newer = Find-Design $Matches[1]
		$older = Find-Design $Matches[2]
		if (-not $newer -or -not $older) { continue }
		if ($newer.id -eq $older.id) { Write-Warning "lines.txt line ${number}: a design cannot replace itself"; continue }
		# would this close a loop?
		$cursor = $older.id; $loop = $false; $hops = 0
		while ($cursor -and $hops++ -lt 1000) { if ($cursor -eq $newer.id) { $loop = $true; break }; $cursor = $replaces[$cursor] }
		if ($loop) { Write-Warning "lines.txt line ${number}: '$($newer.name)' replacing '$($older.name)' would make a loop"; continue }
		$replaces[$newer.id] = $older.id
		$lineCount++
	}
}
Write-Host "Lines: $lineCount design(s) replace another one"

# --- stats/templates.json: the game's own file with the library added as built-in designs that human players get
$entries = @()
foreach ($u in $usable) {
	$d = $u.design
	$entry = [ordered]@{ available = $true; body = $d.body; id = $d.id; name = $d.name }
	foreach ($field in "brain", "ecm", "repair", "sensor", "construct") {
		$value = Get-Part $d $field
		if ($value) { $entry[$field] = $value }
	}
	$entry["propulsion"] = $d.propulsion
	$entry["type"] = $d.type
	$weapons = @(Get-Weapons $d)
	if ($weapons.Count -gt 0) { $entry["weapons"] = $weapons }
	$text = ($entry | ConvertTo-Json -Depth 4 -Compress)
	$entries += "`t`"$($d.id)`": $text"
}
$base = $stockTemplates.TrimEnd()
if (-not $base.EndsWith("}")) { throw "stats/templates.json of the game does not end the way it is expected to" }
$base = $base.Substring(0, $base.Length - 1).TrimEnd()
if ($entries.Count -gt 0) { $templatesJson = $base + ",`n" + ($entries -join ",`n") + "`n}`n" } else { $templatesJson = $stockTemplates }
[void]($templatesJson | ConvertFrom-Json)   # must still be valid JSON

# --- multiplay/script/mods/init.js
$data = @()
foreach ($u in $usable) {
	$d = $u.design
	$row = [ordered]@{ id = $d.id; name = $d.name; parts = @($u.parts) }
	if ($replaces.ContainsKey($d.id)) { $row["replaces"] = $replaces[$d.id] }
	$data += $row
}
$dataJson = ConvertTo-Json -InputObject @($data) -Depth 5
$template = [System.IO.File]::ReadAllText((Join-Path $here "src\init.js.template"), $utf8)
$script = $template.Replace("/*DV_DATA*/[]", $dataJson)

# --- the archive (entry names with forward slashes, as the game expects)
New-Item -ItemType Directory -Force (Split-Path $OutFile) | Out-Null
if (Test-Path $OutFile) { Remove-Item $OutFile -Force }
$archive = [System.IO.Compression.ZipFile]::Open($OutFile, [System.IO.Compression.ZipArchiveMode]::Create)
try {
	foreach ($item in @(@("stats/templates.json", $templatesJson), @("multiplay/script/mods/init.js", $script))) {
		$entry = $archive.CreateEntry($item[0], [System.IO.Compression.CompressionLevel]::Optimal)
		$stream = $entry.Open()
		$bytes = $utf8.GetBytes($item[1])
		$stream.Write($bytes, 0, $bytes.Length)
		$stream.Close()
	}
} finally { $archive.Dispose() }
Write-Host "Wrote $OutFile ($($usable.Count) designs)"
