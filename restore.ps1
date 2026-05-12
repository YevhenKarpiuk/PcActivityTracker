$ErrorActionPreference = "Stop"

$dotnet = & "$PSScriptRoot\scripts\resolve-dotnet-sdk.ps1"
& $dotnet restore .\src\PcActivityTracker.App\PcActivityTracker.App.csproj
exit $LASTEXITCODE
