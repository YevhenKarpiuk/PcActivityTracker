param(
    [string]$Configuration = "Release",
    [switch]$SelfContained,
    [bool]$SingleFile = $true
)

$ErrorActionPreference = "Stop"

$selfContainedValue = if ($SelfContained) { "true" } else { "false" }
$singleFileValue = if ($SingleFile) { "true" } else { "false" }
$dotnet = & "$PSScriptRoot\scripts\resolve-dotnet-sdk.ps1"

& $dotnet publish .\src\PcActivityTracker.App\PcActivityTracker.App.csproj `
    -c $Configuration `
    -r win-x64 `
    --self-contained $selfContainedValue `
    /p:PublishSingleFile=$singleFileValue `
    /p:IncludeNativeLibrariesForSelfExtract=true `
    /p:EnableCompressionInSingleFile=true `
    /p:DebugType=None `
    /p:DebugSymbols=false `
    /p:UseSharedCompilation=false
exit $LASTEXITCODE
