@echo off
setlocal
cd /d "%~dp0"
set "EXE=%~dp0build\windows-release\PcActivityTracker.exe"
if not exist "%EXE%" (
  echo PcActivityTracker.exe not found. Run build_windows.cmd first.
  exit /b 1
)
"%EXE%"
exit /b %ERRORLEVEL%
