param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedVersion
)

$ErrorActionPreference = 'Stop'

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$versionInfo = (Get-Item -LiteralPath $resolvedExecutable).VersionInfo
$expectedWindowsVersion = "$ExpectedVersion.0"

$failures = @()
if (!([string]$versionInfo.FileVersion).StartsWith(
        $expectedWindowsVersion, [System.StringComparison]::Ordinal)) {
    $failures += "FileVersion expected $expectedWindowsVersion, actual '$($versionInfo.FileVersion)'"
}
if (!([string]$versionInfo.ProductVersion).StartsWith(
        $expectedWindowsVersion, [System.StringComparison]::Ordinal)) {
    $failures += "ProductVersion expected $expectedWindowsVersion, actual '$($versionInfo.ProductVersion)'"
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "Version metadata verified: $resolvedExecutable ($expectedWindowsVersion)"
