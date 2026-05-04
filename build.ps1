param(
    [string]$Preset = "build-release-msvc"
)

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere.exe not found. Install Visual Studio with the Desktop development with C++ workload."
    exit 1
}
$installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installPath) {
    Write-Error "Visual Studio with MSVC x64 tools not found (vswhere returned no installation)."
    exit 1
}
$vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
$skse = (Join-Path $PSScriptRoot "skse" | Resolve-Path).Path

$buildPreset = if ($Preset -like "*release*") { "release-msvc" } else { "debug-msvc" }

# Ninja + MSVC need INCLUDE/LIB from vcvars; plain PowerShell does not load them.
cmd /c "`"$vcvars`" && cd /d `"$skse`" && cmake --preset `"$Preset`" && cmake --build --preset `"$buildPreset`""
exit $LASTEXITCODE
