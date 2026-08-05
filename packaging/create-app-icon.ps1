param(
    [string]$SourcePath = (Join-Path $PSScriptRoot '..\assets\modbus-icon.png'),
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\assets\app-icon.ico')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-LogoIconBitmap {
    param(
        [System.Drawing.Image]$Source,
        [int]$Size
    )

    $bitmap = [System.Drawing.Bitmap]::new(
        $Size,
        $Size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $margin = [Math]::Max(1, [int][Math]::Round($Size * 0.03))
        $sourceCrop = [System.Drawing.Rectangle]::new(0, 0, $Source.Width, $Source.Height)
        $availableWidth = $Size - (2 * $margin)
        $availableHeight = $Size - (2 * $margin)
        $scale = [Math]::Min(
            $availableWidth / [double]$sourceCrop.Width,
            $availableHeight / [double]$sourceCrop.Height
        )
        $drawWidth = [Math]::Max(1, [int][Math]::Round($sourceCrop.Width * $scale))
        $drawHeight = [Math]::Max(1, [int][Math]::Round($sourceCrop.Height * $scale))
        $left = [int][Math]::Round(($Size - $drawWidth) / 2)
        $top = [int][Math]::Round(($Size - $drawHeight) / 2)

        $destination = [System.Drawing.Rectangle]::new($left, $top, $drawWidth, $drawHeight)
        $graphics.DrawImage($Source, $destination, $sourceCrop,
                            [System.Drawing.GraphicsUnit]::Pixel)
    }
    finally {
        $graphics.Dispose()
    }
    return $bitmap
}

$resolvedSource = [System.IO.Path]::GetFullPath($SourcePath)
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
if (-not [System.IO.File]::Exists($resolvedSource)) {
    throw "Source image not found: $resolvedSource"
}

[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($resolvedOutput)) | Out-Null
$source = [System.Drawing.Image]::FromFile($resolvedSource)
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = [System.Collections.Generic.List[object]]::new()

try {
    foreach ($size in $sizes) {
        $bitmap = New-LogoIconBitmap -Source $source -Size $size
        $stream = [System.IO.MemoryStream]::new()
        try {
            $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            $images.Add([pscustomobject]@{
                Size = $size
                Data = $stream.ToArray()
            })
        }
        finally {
            $stream.Dispose()
            $bitmap.Dispose()
        }
    }

    $fileStream = [System.IO.File]::Create($resolvedOutput)
    $writer = [System.IO.BinaryWriter]::new($fileStream)
    try {
        $writer.Write([uint16]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]$images.Count)

        $offset = 6 + (16 * $images.Count)
        foreach ($image in $images) {
            $dimension = if ($image.Size -eq 256) { 0 } else { $image.Size }
            $writer.Write([byte]$dimension)
            $writer.Write([byte]$dimension)
            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([uint16]1)
            $writer.Write([uint16]32)
            $writer.Write([uint32]$image.Data.Length)
            $writer.Write([uint32]$offset)
            $offset += $image.Data.Length
        }

        foreach ($image in $images) {
            $writer.Write([byte[]]$image.Data)
        }
    }
    finally {
        $writer.Dispose()
        $fileStream.Dispose()
    }
}
finally {
    $source.Dispose()
}

Write-Host "Created application icon: $resolvedOutput"
