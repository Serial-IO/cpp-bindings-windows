param(
    [Parameter(Mandatory = $true)]
    [string]$Binary,

    [Parameter(Mandatory = $true)]
    [ValidateSet("x86_64-windows-msvc")]
    [string]$Target
)

$ErrorActionPreference = "Stop"

$binaryPath = (Resolve-Path $Binary).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe was not found"
}

$dumpbin = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find "VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" | Select-Object -First 1
if (-not $dumpbin) {
    throw "dumpbin.exe was not found in the Visual Studio installation"
}

$headers = (& $dumpbin /headers $binaryPath 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /headers failed`n$headers"
}
if ($headers -notmatch "(?im)^\s*8664 machine \(x64\)") {
    throw "Expected an x86-64 PE DLL for $Target"
}
if ($headers -notmatch "(?im)^\s*DLL\s*$") {
    throw "Expected a PE DLL, not an executable"
}

$dependents = (& $dumpbin /dependents $binaryPath 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /dependents failed`n$dependents"
}
if ($dependents -match "(?i)(msvcp[^\s]*|vcruntime[^\s]*|ucrtbased)\.dll") {
    throw "Release DLL unexpectedly depends on a dynamic MSVC C/C++ runtime`n$dependents"
}

$exports = (& $dumpbin /exports $binaryPath 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "dumpbin /exports failed`n$exports"
}

$expectedExports = @(
    "getVersion",
    "serialAbortRead",
    "serialAbortWrite",
    "serialClearBufferIn",
    "serialClearBufferOut",
    "serialClose",
    "serialDrain",
    "serialGetBaudrate",
    "serialGetCts",
    "serialGetDataBits",
    "serialGetDcd",
    "serialGetDsr",
    "serialGetFlowControl",
    "serialGetParity",
    "serialGetRi",
    "serialGetStopBits",
    "serialInBytesTotal",
    "serialInBytesWaiting",
    "serialListPorts",
    "serialMonitorPorts",
    "serialOpen",
    "serialOutBytesTotal",
    "serialOutBytesWaiting",
    "serialRead",
    "serialReadLine",
    "serialReadUntil",
    "serialReadUntilSequence",
    "serialSendBreak",
    "serialSetBaudrate",
    "serialSetDataBits",
    "serialSetDtr",
    "serialSetErrorCallback",
    "serialSetFlowControl",
    "serialSetParity",
    "serialSetReadCallback",
    "serialSetRts",
    "serialSetStopBits",
    "serialSetWriteCallback",
    "serialWrite"
)

$missingExports = @($expectedExports | Where-Object { $exports -notmatch "(?m)\s$([regex]::Escape($_))\s*$" })
if ($missingExports.Count -ne 0) {
    throw "Release DLL is missing exports: $($missingExports -join ', ')"
}

Write-Host "Verified $Target DLL: x86-64, static MSVC runtime, and $($expectedExports.Count) C API exports"
