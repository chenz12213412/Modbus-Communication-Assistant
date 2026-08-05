param(
    [string]$QtBinDirectory = 'D:\Qt\6.10.2\mingw_64\bin',
    [string]$OutputExecutable = ''
)

$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$buildExecutable = Join-Path $projectRoot 'build\release\ModbusSerialAssistant.exe'
$outputExecutable = if ([string]::IsNullOrWhiteSpace($OutputExecutable)) {
    Join-Path $projectRoot 'dist\ModbusSerialAssistant_OneFile.exe'
} else {
    [System.IO.Path]::GetFullPath($OutputExecutable)
}
$applicationIcon = Join-Path $projectRoot 'assets\app-icon.ico'
$windeployqt = Join-Path $QtBinDirectory 'windeployqt.exe'

$toolRoot = Join-Path ([System.IO.Path]::GetTempPath()) 'ModbusOneFileTools-2602'
$sevenZr = Join-Path $toolRoot '7zr.exe'
$extraArchive = Join-Path $toolRoot '7z-extra.7z'
$sdkArchive = Join-Path $toolRoot 'lzma-sdk.7z'
$extraRoot = Join-Path $toolRoot 'extra'
$sdkRoot = Join-Path $toolRoot 'sdk'
$sevenZa = Join-Path $extraRoot 'x64\7za.exe'
$sfxModule = Join-Path $sdkRoot 'bin\7zS2.sfx'

$resourceHackerRoot = Join-Path ([System.IO.Path]::GetTempPath()) 'ResourceHacker-5.2.8'
$resourceHackerArchive = Join-Path $resourceHackerRoot 'resource_hacker.zip'
$resourceHackerDirectory = Join-Path $resourceHackerRoot 'app'
$resourceHacker = Join-Path $resourceHackerDirectory 'ResourceHacker.exe'

$stagingRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    'ModbusOneFileBuild-' + [System.Guid]::NewGuid().ToString('N'))
$payloadDirectory = Join-Path $stagingRoot 'payload'
$applicationArchive = Join-Path $stagingRoot 'application.7z'
$sfxConfiguration = Join-Path $stagingRoot 'sfx-config.txt'
$stagingIcon = Join-Path $stagingRoot 'app-icon.ico'
$brandedSfxModule = Join-Path $stagingRoot '7zS2-modbus.sfx'
$versionResourceScript = Join-Path $stagingRoot 'app-version.rc'
$compiledVersionResource = Join-Path $stagingRoot 'app-version.res'
$unversionedSfxModule = Join-Path $stagingRoot '7zS2-modbus-no-version.sfx'
$versionedSfxModule = Join-Path $stagingRoot '7zS2-modbus-versioned.sfx'
$temporaryOutput = Join-Path $stagingRoot 'ModbusSerialAssistant_OneFile.exe'

foreach ($requiredFile in @($buildExecutable, $windeployqt, $applicationIcon)) {
    if (!(Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required packaging file is missing: $requiredFile"
    }
}

New-Item -ItemType Directory -Path $toolRoot -Force | Out-Null
if (!(Test-Path -LiteralPath $sevenZr)) {
    Invoke-WebRequest `
        -Uri 'https://github.com/ip7z/7zip/releases/download/26.02/7zr.exe' `
        -OutFile $sevenZr
}
if (!(Test-Path -LiteralPath $extraArchive)) {
    Invoke-WebRequest `
        -Uri 'https://github.com/ip7z/7zip/releases/download/26.02/7z2602-extra.7z' `
        -OutFile $extraArchive
}
if (!(Test-Path -LiteralPath $sdkArchive)) {
    Invoke-WebRequest `
        -Uri 'https://github.com/ip7z/7zip/releases/download/26.02/lzma2602.7z' `
        -OutFile $sdkArchive
}
if (!(Test-Path -LiteralPath $sevenZa)) {
    New-Item -ItemType Directory -Path $extraRoot -Force | Out-Null
    & $sevenZr x $extraArchive "-o$extraRoot" -y | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to extract 7-Zip Extra. Exit code: $LASTEXITCODE"
    }
}
if (!(Test-Path -LiteralPath $sfxModule)) {
    New-Item -ItemType Directory -Path $sdkRoot -Force | Out-Null
    & $sevenZr x $sdkArchive "-o$sdkRoot" -y | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to extract the LZMA SDK. Exit code: $LASTEXITCODE"
    }
}

New-Item -ItemType Directory -Path $resourceHackerRoot -Force | Out-Null
if (!(Test-Path -LiteralPath $resourceHackerArchive)) {
    Invoke-WebRequest `
        -Uri 'https://www.angusj.com/resourcehacker/resource_hacker.zip' `
        -OutFile $resourceHackerArchive
}
if (!(Test-Path -LiteralPath $resourceHacker)) {
    Expand-Archive -LiteralPath $resourceHackerArchive `
        -DestinationPath $resourceHackerDirectory -Force
}

New-Item -ItemType Directory -Path $payloadDirectory | Out-Null
Copy-Item -LiteralPath $buildExecutable `
    -Destination (Join-Path $payloadDirectory 'ModbusSerialAssistant.exe')

$env:QTFRAMEWORK_BYPASS_LICENSE_CHECK = '1'
& $windeployqt --release --no-translations --compiler-runtime `
    (Join-Path $payloadDirectory 'ModbusSerialAssistant.exe')
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed. Exit code: $LASTEXITCODE"
}

Push-Location $payloadDirectory
try {
    & $sevenZa a -t7z $applicationArchive '.\*' -mx=9 -m0=LZMA2 -mmt=on | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "7-Zip compression failed. Exit code: $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

$configuration = @'
;!@Install@!UTF-8!
Title="Modbus Communications Assistant"
BeginPrompt=""
RunProgram="ModbusSerialAssistant.exe"
GUIMode="2"
;!@InstallEnd@!
'@
[System.IO.File]::WriteAllText(
    $sfxConfiguration,
    $configuration,
    [System.Text.UTF8Encoding]::new($false)
)

Copy-Item -LiteralPath $applicationIcon -Destination $stagingIcon
$resourceHackerArguments = @(
    '-open', $sfxModule,
    '-save', $brandedSfxModule,
    '-action', 'addoverwrite',
    '-resource', $stagingIcon,
    '-mask', 'ICONGROUP,1,',
    '-log', 'NUL'
)
$resourceHackerProcess = Start-Process `
    -FilePath $resourceHacker `
    -ArgumentList $resourceHackerArguments `
    -Wait -PassThru -WindowStyle Hidden
if ($resourceHackerProcess.ExitCode -ne 0 -or
    !(Test-Path -LiteralPath $brandedSfxModule -PathType Leaf)) {
    throw 'Failed to apply the application icon to the one-file launcher.'
}

$extractVersionArguments = @(
    '-open', $buildExecutable,
    '-save', $versionResourceScript,
    '-action', 'extract',
    '-mask', 'VERSIONINFO,,',
    '-log', 'NUL'
)
$extractVersionProcess = Start-Process `
    -FilePath $resourceHacker `
    -ArgumentList $extractVersionArguments `
    -Wait -PassThru -WindowStyle Hidden
if ($extractVersionProcess.ExitCode -ne 0 -or
    !(Test-Path -LiteralPath $versionResourceScript -PathType Leaf)) {
    throw 'Failed to extract version metadata from the application.'
}

$compileVersionArguments = @(
    '-open', $versionResourceScript,
    '-save', $compiledVersionResource,
    '-action', 'compile',
    '-log', 'NUL'
)
$compileVersionProcess = Start-Process `
    -FilePath $resourceHacker `
    -ArgumentList $compileVersionArguments `
    -Wait -PassThru -WindowStyle Hidden
if ($compileVersionProcess.ExitCode -ne 0 -or
    !(Test-Path -LiteralPath $compiledVersionResource -PathType Leaf)) {
    throw 'Failed to compile the application version metadata.'
}

$deleteVersionArguments = @(
    '-open', $brandedSfxModule,
    '-save', $unversionedSfxModule,
    '-action', 'delete',
    '-mask', 'VERSIONINFO,,',
    '-log', 'NUL'
)
$deleteVersionProcess = Start-Process `
    -FilePath $resourceHacker `
    -ArgumentList $deleteVersionArguments `
    -Wait -PassThru -WindowStyle Hidden
if ($deleteVersionProcess.ExitCode -ne 0 -or
    !(Test-Path -LiteralPath $unversionedSfxModule -PathType Leaf)) {
    throw 'Failed to remove the original SFX version metadata.'
}

$importVersionArguments = @(
    '-open', $unversionedSfxModule,
    '-save', $versionedSfxModule,
    '-action', 'addoverwrite',
    '-resource', $compiledVersionResource,
    '-mask', 'VERSIONINFO,,,',
    '-log', 'NUL'
)
$importVersionProcess = Start-Process `
    -FilePath $resourceHacker `
    -ArgumentList $importVersionArguments `
    -Wait -PassThru -WindowStyle Hidden
if ($importVersionProcess.ExitCode -ne 0 -or
    !(Test-Path -LiteralPath $versionedSfxModule -PathType Leaf)) {
    throw 'Failed to apply version metadata to the one-file launcher.'
}

$applicationVersion = (Get-Item -LiteralPath $buildExecutable).VersionInfo.FileVersion
$launcherVersion = (Get-Item -LiteralPath $versionedSfxModule).VersionInfo.FileVersion
if ([string]::IsNullOrWhiteSpace($applicationVersion) -or
    $launcherVersion -ne $applicationVersion) {
    throw "One-file launcher version mismatch: expected '$applicationVersion', actual '$launcherVersion'."
}

$outputStream = [System.IO.File]::Create($temporaryOutput)
try {
    foreach ($part in @($versionedSfxModule, $sfxConfiguration, $applicationArchive)) {
        $inputStream = [System.IO.File]::OpenRead($part)
        try {
            $inputStream.CopyTo($outputStream)
        }
        finally {
            $inputStream.Dispose()
        }
    }
}
finally {
    $outputStream.Dispose()
}

Copy-Item -LiteralPath $temporaryOutput -Destination $outputExecutable -Force
$output = Get-Item -LiteralPath $outputExecutable
Write-Output "One-file package completed: $($output.FullName)"
Write-Output "File size: $([Math]::Round($output.Length / 1MB, 1)) MB"
