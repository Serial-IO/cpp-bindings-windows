# Sets the version field in jsr.json (same behavior as Linux set_version.sh).
# Usage: .\set_version.ps1 <JsonFilePath> <version>

param(
    [Parameter(Mandatory = $true)][string]$JsonFilePath,
    [Parameter(Mandatory = $true)][string]$Version
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $JsonFilePath)) {
    throw "Error: Json file not found: $JsonFilePath"
}

$content = Get-Content -Raw -LiteralPath $JsonFilePath
$newContent = $content -replace '"version":\s*""', "`"version`": `"$Version`""
Set-Content -Path $JsonFilePath -Value $newContent -NoNewline
