# Starts the Design Vault build of Warzone 2100. It uses your normal settings, saves and stored designs (the same
# folder as the Steam version) and the game data that is linked into dist\ by package.ps1. Any extra arguments are
# passed on to the game, for example:  .\play.ps1 --window --resolution=1600x900
$root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))   # the repository root
$exe = Join-Path $root "dist\bin\warzone2100.exe"
if (-not (Test-Path $exe)) { throw "Run design-vault\scripts\package.ps1 first" }
$start = @{ FilePath = $exe; WorkingDirectory = (Split-Path $exe) }
if ($args.Count -gt 0) { $start.ArgumentList = $args }   # Start-Process rejects an empty list
Start-Process @start
