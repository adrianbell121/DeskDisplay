param([ValidateRange(1024, 65535)][int]$Port = 8765)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$pythonPath = Join-Path $projectRoot '.tools/host-python/Scripts/python.exe'
if (-not (Test-Path -LiteralPath $pythonPath)) {
    $pythonPath = (Get-Command python -ErrorAction Stop).Source
}
Write-Host "DeskDisplay Face Lab: http://127.0.0.1:$Port"
Write-Host 'Press Ctrl+C to stop. Designs are saved in the shared data folder; Export workspace creates a portable copy.'
& $pythonPath (Join-Path $projectRoot 'scripts/preview_server.py') --port $Port
exit $LASTEXITCODE
