$ErrorActionPreference = "Stop"

$dotnet = & "$PSScriptRoot\scripts\resolve-dotnet-sdk.ps1"
& $dotnet build .\src\PcActivityTracker.App\PcActivityTracker.App.csproj --no-restore /p:UseSharedCompilation=false
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $dotnet run --project .\src\PcActivityTracker.App\PcActivityTracker.App.csproj --no-build
exit $LASTEXITCODE
