param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    $version = & $Python -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")'
    if ($LASTEXITCODE -ne 0 -or $version -ne '3.11') {
        throw 'Use Python 3.11: .\scripts\setup.ps1 -Python C:\path\to\python.exe'
    }
    New-Item -ItemType Directory -Path '.tools' -Force | Out-Null
    if (-not (Test-Path -LiteralPath '.tools\esp-idf\tools\idf.py')) {
        & git clone --depth 1 --branch v5.5.2 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git .tools/esp-idf
        if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF download failed.' }
    }
    $env:IDF_TOOLS_PATH = Join-Path $projectRoot '.tools\espressif'
    & $Python .tools/esp-idf/tools/idf_tools.py install --targets esp32s3
    if ($LASTEXITCODE -ne 0) { throw 'Compiler installation failed.' }
    & $Python .tools/esp-idf/tools/idf_tools.py install-python-env
    if ($LASTEXITCODE -ne 0) { throw 'IDF Python installation failed.' }
    if (-not (Test-Path -LiteralPath '.tools\host-python\Scripts\python.exe')) {
        & $Python -m venv .tools/host-python
        if ($LASTEXITCODE -ne 0) { throw 'Host environment creation failed.' }
    }
    & .tools/host-python/Scripts/python.exe -m pip install esptool==4.11.0 pyserial==3.5
    if ($LASTEXITCODE -ne 0) { throw 'Host tools installation failed.' }
    Write-Host 'Setup complete. Run .\scripts\idf.ps1 build.'
} finally { Pop-Location }
