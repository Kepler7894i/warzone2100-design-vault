# Builds and runs the standalone unit tests. Needs only a C++17 compiler (MSVC from a VS developer prompt, or g++).
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path (Split-Path -Parent (Split-Path -Parent $here)) "src"
$out = Join-Path $here "out"
New-Item -ItemType Directory -Force $out | Out-Null

$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($gpp) {
	& g++ -std=c++17 -Wall -Wextra -Werror -I $src (Join-Path $here "designchain_test.cpp") -o (Join-Path $out "designchain_test.exe")
} else {
	$vcvars = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
	cmd /c "`"$vcvars`" >nul && cl /nologo /std:c++17 /W4 /WX /EHsc /I `"$src`" `"$(Join-Path $here 'designchain_test.cpp')`" /Fo:`"$out\`" /Fe:`"$(Join-Path $out 'designchain_test.exe')`""
}
if ($LASTEXITCODE -ne 0) { throw "compile failed" }
& (Join-Path $out "designchain_test.exe")
if ($LASTEXITCODE -ne 0) { throw "tests failed" }
