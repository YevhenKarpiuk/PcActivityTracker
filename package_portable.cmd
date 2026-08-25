@echo off
setlocal EnableExtensions
cd /d "%~dp0"

rem Builds the portable ZIP from an existing Release build.
rem Run build_windows.cmd first.
rem Keep this file ASCII-only: cmd.exe reads batch files in the OEM code page.
rem
rem Staging happens in dist\.stage so the archive can only ever contain files placed here by
rem this script. An unpacked copy in dist\<name> may be a running installation with its own
rem data directory: it is never touched and never ends up inside the archive.

set "OUT=%~dp0build\windows-release"
set "DIST=%~dp0dist"
set "STAGEROOT=%DIST%\.stage"

if not exist "%OUT%\PcActivityTracker.exe" (
  echo [ERROR] %OUT%\PcActivityTracker.exe not found. Run build_windows.cmd first.
  exit /b 1
)

set "VER=0.0.0"
for /f "tokens=2 delims==" %%V in ('findstr /b /c:"CMAKE_PROJECT_VERSION:" "%OUT%\CMakeCache.txt" 2^>nul') do set "VER=%%V"

set "NAME=PcActivityTracker-%VER%-win-x64-portable"
set "STAGE=%STAGEROOT%\%NAME%"
set "ZIP=%DIST%\%NAME%.zip"

if exist "%STAGEROOT%" rmdir /s /q "%STAGEROOT%"
if exist "%STAGEROOT%" (
  echo [ERROR] cannot clear staging directory %STAGEROOT%
  echo A file there is locked. Close anything running from that path and retry.
  exit /b 2
)
mkdir "%STAGE%" 2>nul
if not exist "%STAGE%" (
  echo [ERROR] cannot create staging directory %STAGE%
  exit /b 2
)

echo Staging %NAME% ...
for %%F in (PcActivityTracker.exe SDL3.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll) do (
  if not exist "%OUT%\%%F" (
    echo [ERROR] missing build artifact: %OUT%\%%F
    exit /b 3
  )
  copy /y "%OUT%\%%F" "%STAGE%\%%F" >nul
  if errorlevel 1 (
    echo [ERROR] cannot copy %%F into the staging directory.
    exit /b 3
  )
)
for %%F in (portable.txt README.txt) do (
  copy /y "%~dp0resources\portable\%%F" "%STAGE%\%%F" >nul
  if errorlevel 1 (
    echo [ERROR] cannot copy resources\portable\%%F
    exit /b 3
  )
)

rem The staged tree must contain exactly the seven files above and nothing else.
set "STAGED=0"
for /f %%C in ('dir /b /s /a-d "%STAGE%" ^| find /c /v ""') do set "STAGED=%%C"
if not "%STAGED%"=="7" (
  echo [ERROR] staging directory holds %STAGED% files, expected 7.
  exit /b 4
)

echo Compressing ...
if exist "%ZIP%" del /q "%ZIP%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%STAGE%' -DestinationPath '%ZIP%' -CompressionLevel Optimal -Force"
if errorlevel 1 (
  echo [ERROR] Compress-Archive failed.
  exit /b 5
)
if not exist "%ZIP%" (
  echo [ERROR] archive was not created.
  exit /b 5
)

rmdir /s /q "%STAGEROOT%" 2>nul

echo.
echo SUCCESS
echo ZIP: %ZIP%
if exist "%DIST%\%NAME%\" echo NOTE: %DIST%\%NAME%\ is an unpacked copy and was left untouched.
exit /b 0
