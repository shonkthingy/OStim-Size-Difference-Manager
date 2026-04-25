param(
    [string]$Preset = "build-debug-msvc"
)

Push-Location "$PSScriptRoot\skse"
try {
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if ($Preset -like "*release*") {
        cmake --build --preset release-msvc
    } else {
        cmake --build --preset debug-msvc
    }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
finally {
    Pop-Location
}
