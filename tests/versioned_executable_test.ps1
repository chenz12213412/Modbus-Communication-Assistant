param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [Parameter(Mandatory = $true)]
    [string]$ExpectedVersion,

    [string]$ExpectedFileName = ''
)

$ErrorActionPreference = 'Stop'

$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$versionInfo = (Get-Item -LiteralPath $resolvedExecutable).VersionInfo
$expectedWindowsVersion = "$ExpectedVersion.0"

$failures = @()
if (![string]::IsNullOrWhiteSpace($ExpectedFileName) -and
    [System.IO.Path]::GetFileName($resolvedExecutable) -ne $ExpectedFileName) {
    $failures += "File name expected '$ExpectedFileName', actual '$([System.IO.Path]::GetFileName($resolvedExecutable))'"
}
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
