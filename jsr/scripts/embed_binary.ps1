param(
    [Parameter(Mandatory = $true)][string]$BinaryPath,
    [Parameter(Mandatory = $true)][string]$JsrBinPath,
    [Parameter(Mandatory = $true)][string]$Target
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $BinaryPath)) {
    throw "Error: Binary path does not exist: $BinaryPath"
}

$item = Get-Item -LiteralPath $BinaryPath
if ($item.PSIsContainer) {
    throw "Error: Binary path is a directory: $BinaryPath"
}

New-Item -ItemType Directory -Force -Path $JsrBinPath | Out-Null

$filename = [System.IO.Path]::GetFileName($BinaryPath)

# Copy dll into the package bin folder with a stable name
Copy-Item -Force -LiteralPath $BinaryPath -Destination (Join-Path $JsrBinPath "x86_64.dll")

$bytes = [System.IO.File]::ReadAllBytes($BinaryPath)
$b64 = [System.Convert]::ToBase64String($bytes)

$obj = @{
    target   = $Target
    filename = $filename
    encoding = "base64"
    data     = $b64
}

$json = ($obj | ConvertTo-Json -Compress)
$outPath = Join-Path $JsrBinPath "x86_64.json"

Set-Content -Path $outPath -Value $json -NoNewline -Encoding utf8NoBOM


