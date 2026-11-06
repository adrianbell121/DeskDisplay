param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$url = 'http://127.0.0.1:8765/'
$boardAddress = Join-Path $projectRoot 'data\board-dashboard.url'
$boardHosted = Test-Path -LiteralPath $boardAddress
if ($boardHosted) { $url = (Get-Content -LiteralPath $boardAddress -Raw).Trim().TrimEnd('/') + '/' }
function Test-Dashboard {
    try {
        $result = Invoke-RestMethod ($url + 'api/capabilities') -TimeoutSec 2
        return $result.sceneVersion -eq 2
    } catch { return $false }
}
try {
    if (-not (Test-Dashboard)) {
        if ($boardHosted) { throw "The clock is not reachable at $url Keep it powered and connect this computer to the same Wi-Fi. If its address changed, update data\board-dashboard.url using the address reported by the clock's USB web command." }
        $pythonPath = Join-Path $projectRoot '.tools\host-python\Scripts\python.exe'
        if (-not (Test-Path -LiteralPath $pythonPath)) { throw 'Dashboard Python runtime is missing. Keep the DeskDisplay folder in its original location.' }
        $serverPath = Join-Path $projectRoot 'scripts\preview_server.py'
        Start-Process -FilePath $pythonPath -ArgumentList @(('"' + $serverPath + '"'), '--port', '8765') -WorkingDirectory $projectRoot -WindowStyle Hidden
        $deadline = (Get-Date).AddSeconds(20)
        while (-not (Test-Dashboard)) {
            if ((Get-Date) -gt $deadline) { throw 'Dashboard could not start. Port 8765 may be in use by another application.' }
            Start-Sleep -Milliseconds 300
        }
    }
    if ($CheckOnly) { Write-Output "Dashboard ready at $url" }
    else { Start-Process $url }
} catch {
    Add-Type -AssemblyName PresentationFramework
    [System.Windows.MessageBox]::Show($_.Exception.Message, 'DeskDisplay dashboard') | Out-Null
    exit 1
}
