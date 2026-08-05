param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version,

    [string]$QtBinDirectory = 'D:\Qt\6.10.2\mingw_64\bin'
)

$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildExecutable = Join-Path $projectRoot 'build\release\ModbusSerialAssistant.exe'
$distDirectory = Join-Path $projectRoot 'dist'
$versionedExecutable = Join-Path $distDirectory "ModbusSerialAssistant_v$Version.exe"
$versionedOneFile = Join-Path $distDirectory "ModbusSerialAssistant_v${Version}_OneFile.exe"
$latestExecutable = Join-Path $distDirectory 'ModbusSerialAssistant.exe'
$latestOneFile = Join-Path $distDirectory 'ModbusSerialAssistant_OneFile.exe'
$oneFileScript = Join-Path $PSScriptRoot 'onefile\build-onefile.ps1'
$temporaryOneFile = Join-Path ([System.IO.Path]::GetTempPath()) (
    "ModbusSerialAssistant_v${Version}_" + [System.Guid]::NewGuid().ToString('N') + '.exe')
$expectedWindowsVersion = "$Version.0"

function Assert-ExecutableVersion {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $versionInfo = (Get-Item -LiteralPath $Path).VersionInfo
    if ($versionInfo.FileVersion -ne $expectedWindowsVersion -or
        $versionInfo.ProductVersion -ne $expectedWindowsVersion) {
        throw "Executable version mismatch for '$Path': expected '$expectedWindowsVersion', " +
              "actual FileVersion '$($versionInfo.FileVersion)', " +
              "ProductVersion '$($versionInfo.ProductVersion)'."
    }
}

foreach ($requiredFile in @($buildExecutable, $oneFileScript)) {
    if (!(Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required release file is missing: $requiredFile"
    }
}

Assert-ExecutableVersion -Path $buildExecutable
New-Item -ItemType Directory -Path $distDirectory -Force | Out-Null

foreach ($archivePath in @($versionedExecutable, $versionedOneFile)) {
    if (Test-Path -LiteralPath $archivePath) {
        throw "Versioned release already exists and will not be overwritten: $archivePath"
    }
}

try {
    & $oneFileScript `
        -QtBinDirectory $QtBinDirectory `
        -OutputExecutable $temporaryOneFile
    if ($LASTEXITCODE -ne 0) {
        throw "One-file packaging failed. Exit code: $LASTEXITCODE"
    }

    Assert-ExecutableVersion -Path $temporaryOneFile

    Copy-Item -LiteralPath $buildExecutable -Destination $versionedExecutable
    Copy-Item -LiteralPath $temporaryOneFile -Destination $versionedOneFile

    Copy-Item -LiteralPath $versionedExecutable -Destination $latestExecutable -Force
    Copy-Item -LiteralPath $versionedOneFile -Destination $latestOneFile -Force

    $windeployqt = Join-Path $QtBinDirectory 'windeployqt.exe'
    & $windeployqt --release --no-translations --compiler-runtime $latestExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "windeployqt failed. Exit code: $LASTEXITCODE"
    }
}
finally {
    Remove-Item -LiteralPath $temporaryOneFile -Force -ErrorAction SilentlyContinue
}

Write-Output "Versioned release completed:"
Write-Output "  $versionedExecutable"
Write-Output "  $versionedOneFile"
