param([switch]$CheckOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$url = 'http://127.0.0.1:8765/'
function Test-Dashboard {
    try {
        $result = Invoke-RestMethod ($url + 'api/capabilities') -TimeoutSec 2
        return $result.sceneVersion -eq 2
    } catch { return $false }
}
try {
    if (-not (Test-Dashboard)) {
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
    if ($CheckOnly) { Write-Output 'Dashboard ready; existing server reused when available.' }
    else { Start-Process $url }
} catch {
    Add-Type -AssemblyName PresentationFramework
    [System.Windows.MessageBox]::Show($_.Exception.Message, 'DeskDisplay dashboard') | Out-Null
    exit 1
}
