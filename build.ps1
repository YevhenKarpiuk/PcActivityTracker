param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$dotnet = & "$PSScriptRoot\scripts\resolve-dotnet-sdk.ps1"
$assetsPath = Join-Path $PSScriptRoot "src\PcActivityTracker.App\obj\project.assets.json"
if (-not (Test-Path $assetsPath)) {
    & $dotnet restore .\src\PcActivityTracker.App\PcActivityTracker.App.csproj /p:NuGetAudit=false
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $dotnet build .\PcActivityTracker.sln -c $Configuration --no-restore -m:1 /p:UseSharedCompilation=false
exit $LASTEXITCODE
