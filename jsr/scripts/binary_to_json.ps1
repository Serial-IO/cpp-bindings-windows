param(
    [Parameter(Mandatory = $true)][string]$BinaryFilePath,
    [Parameter(Mandatory = $true)][string]$JsonFilePath,
    [Parameter(Mandatory = $true)][string]$Target
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $BinaryFilePath) -or (Get-Item -LiteralPath $BinaryFilePath).PSIsContainer) {
    Write-Error "Error: Binary path is not a file: $BinaryFilePath"
    exit 1
}

$filename = [System.IO.Path]::GetFileName($BinaryFilePath)

New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($JsonFilePath)) | Out-Null

$bytes = [System.IO.File]::ReadAllBytes($BinaryFilePath)
$b64 = [System.Convert]::ToBase64String($bytes)

$obj = [ordered]@{
    target   = $Target
    filename = $filename
    encoding = "base64"
    data     = $b64
}

$json = ($obj | ConvertTo-Json -Compress)

Set-Content -Path $JsonFilePath -Value $json -NoNewline -Encoding utf8NoBOM
