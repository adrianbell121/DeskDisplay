# Run from a normal VS Code PowerShell: .\scripts\idf.ps1 build
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$env:IDF_PATH = Join-Path $projectRoot '.tools\esp-idf'
$env:IDF_TOOLS_PATH = Join-Path $projectRoot '.tools\espressif'
$idfPython = Join-Path $env:IDF_TOOLS_PATH 'python_env\idf5.5_py3.11_env\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $idfPython)) { throw 'ESP-IDF is not installed. See docs/firmware.md.' }
# idf_tools supplies the compiler/CMake/Ninja paths; avoid depending on shell activation.
$exports = & $idfPython "$env:IDF_PATH\tools\idf_tools.py" export --format key-value
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF tool environment failed.' }
foreach ($entry in $exports) {
    if ($entry -match '^([^=]+)=(.*)$') {
        $key = $Matches[1]
        $value = $Matches[2].Replace('%PATH%', $env:PATH).Replace('$PATH', $env:PATH)
        [Environment]::SetEnvironmentVariable($key, $value, 'Process')
    }
}
$env:PATH = "$(Split-Path $idfPython -Parent);$env:PATH"
Push-Location $projectRoot
try { & $idfPython "$env:IDF_PATH\tools\idf.py" @args; $result = $LASTEXITCODE }
finally { Pop-Location }
exit $result
