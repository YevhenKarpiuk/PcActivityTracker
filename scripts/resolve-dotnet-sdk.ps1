$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$env:DOTNET_CLI_HOME = Join-Path $projectRoot ".dotnet-home"
$env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = "1"
$env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"
$env:DOTNET_ADD_GLOBAL_TOOLS_TO_PATH = "0"
$env:DOTNET_NOLOGO = "1"

$candidates = @()
$toolsRoot = "C:\tools\dotnet"
if (Test-Path $toolsRoot) {
    $candidates += Get-ChildItem -Path $toolsRoot -Filter dotnet.exe -File -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        Select-Object -ExpandProperty FullName
}

$pathDotnet = Get-Command dotnet -ErrorAction SilentlyContinue
if ($pathDotnet) {
    $candidates += $pathDotnet.Source
}

foreach ($candidate in ($candidates | Select-Object -Unique)) {
    $sdks = & $candidate --list-sdks 2>$null
    if ($LASTEXITCODE -eq 0 -and $sdks) {
        Write-Output $candidate
        return
    }
}

throw "No .NET SDK was found. Install .NET SDK 10.x or put it under C:\tools\dotnet."
