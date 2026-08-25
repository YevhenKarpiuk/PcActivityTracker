@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

rem Windows acceptance build using the local toolchain in C:\tools.
rem Override with environment variables PCAT_TOOLS_ROOT / PCAT_MINGW_ROOT.
rem Argument "incremental" skips --fresh and reuses the existing CMake cache.
rem Keep this file ASCII-only: cmd.exe reads batch files in the OEM code page.

if not defined PCAT_TOOLS_ROOT set "PCAT_TOOLS_ROOT=C:\tools"
if not defined PCAT_MINGW_ROOT set "PCAT_MINGW_ROOT=%PCAT_TOOLS_ROOT%\mingw64"

set "FRESH=--fresh"
if /I "%~1"=="incremental" set "FRESH="

if not exist "%PCAT_MINGW_ROOT%\bin\g++.exe" (
  echo [ERROR] MinGW-w64 UCRT compiler not found: %PCAT_MINGW_ROOT%\bin\g++.exe
  echo Install a UCRT64 GCC toolchain there or set PCAT_MINGW_ROOT. See WINDOWS_BUILD.md.
  exit /b 1
)
for %%T in (gcc.exe windres.exe ar.exe) do (
  if not exist "%PCAT_MINGW_ROOT%\bin\%%T" (
    echo [ERROR] %%T not found in %PCAT_MINGW_ROOT%\bin
    exit /b 1
  )
)

set "PATH=%PCAT_MINGW_ROOT%\bin;%PATH%"

rem CMake: prefer the dedicated C:\tools\cmake, then the toolchain copy, then PATH.
set "PCAT_CMAKE="
if exist "%PCAT_TOOLS_ROOT%\cmake\bin\cmake.exe" set "PCAT_CMAKE=%PCAT_TOOLS_ROOT%\cmake\bin\cmake.exe"
if not defined PCAT_CMAKE if exist "%PCAT_MINGW_ROOT%\bin\cmake.exe" set "PCAT_CMAKE=%PCAT_MINGW_ROOT%\bin\cmake.exe"
if not defined PCAT_CMAKE for /f "delims=" %%I in ('where cmake 2^>nul') do if not defined PCAT_CMAKE set "PCAT_CMAKE=%%I"
if not defined PCAT_CMAKE (
  echo [ERROR] cmake.exe not found. Expected %PCAT_TOOLS_ROOT%\cmake\bin\cmake.exe or cmake in PATH.
  exit /b 1
)
for %%I in ("%PCAT_CMAKE%") do set "PCAT_CMAKE_BIN=%%~dpI"
set "PCAT_CTEST=%PCAT_CMAKE_BIN%ctest.exe"
if not exist "%PCAT_CTEST%" (
  echo [ERROR] ctest.exe not found next to %PCAT_CMAKE%
  exit /b 1
)

rem Ninja: prefer C:\tools\ninja, then the toolchain copy, then PATH.
set "PCAT_NINJA="
if exist "%PCAT_TOOLS_ROOT%\ninja\ninja.exe" set "PCAT_NINJA=%PCAT_TOOLS_ROOT%\ninja\ninja.exe"
if not defined PCAT_NINJA if exist "%PCAT_MINGW_ROOT%\bin\ninja.exe" set "PCAT_NINJA=%PCAT_MINGW_ROOT%\bin\ninja.exe"
if not defined PCAT_NINJA for /f "delims=" %%I in ('where ninja 2^>nul') do if not defined PCAT_NINJA set "PCAT_NINJA=%%I"
if not defined PCAT_NINJA (
  echo [ERROR] ninja.exe not found. Expected %PCAT_TOOLS_ROOT%\ninja\ninja.exe or ninja in PATH.
  exit /b 1
)

rem Git is required by FetchContent for SDL3/ImGui/ImPlot.
where git >nul 2>nul
if errorlevel 1 if exist "%ProgramFiles%\Git\cmd\git.exe" set "PATH=%ProgramFiles%\Git\cmd;%PATH%"
where git >nul 2>nul
if errorlevel 1 (
  echo [ERROR] git.exe not found in PATH. FetchContent cannot download SDL3/ImGui/ImPlot.
  exit /b 1
)

set "PCAT_MINGW_CM=%PCAT_MINGW_ROOT:\=/%"
set "PCAT_NINJA_CM=%PCAT_NINJA:\=/%"

echo Toolchain:
echo   g++    : %PCAT_MINGW_ROOT%\bin\g++.exe
echo   cmake  : %PCAT_CMAKE%
echo   ninja  : %PCAT_NINJA%
if exist "%PCAT_MINGW_ROOT%\include\sqlite3.h" (
  echo   sqlite3: toolchain header %PCAT_MINGW_ROOT%\include\sqlite3.h
) else (
  echo   sqlite3: amalgamation fetched by CMake, see cmake\SQLite3.cmake
)
echo.

echo [1/4] Configure Windows Release...
"%PCAT_CMAKE%" --preset windows-release %FRESH% -DCMAKE_C_COMPILER="%PCAT_MINGW_CM%/bin/gcc.exe" -DCMAKE_CXX_COMPILER="%PCAT_MINGW_CM%/bin/g++.exe" -DCMAKE_RC_COMPILER="%PCAT_MINGW_CM%/bin/windres.exe" -DCMAKE_MAKE_PROGRAM="%PCAT_NINJA_CM%" -DCMAKE_PREFIX_PATH="%PCAT_MINGW_CM%"
if errorlevel 1 exit /b 1

echo [2/4] Build...
"%PCAT_CMAKE%" --build --preset windows-release
if errorlevel 1 exit /b 1

echo [3/4] Tests...
"%PCAT_CTEST%" --preset windows-release
if errorlevel 1 exit /b 1

echo [4/4] Copy runtime DLLs next to executable...
set "OUT=%~dp0build\windows-release"
for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libsqlite3-0.dll) do (
  if exist "%PCAT_MINGW_ROOT%\bin\%%D" copy /y "%PCAT_MINGW_ROOT%\bin\%%D" "%OUT%\%%D" >nul
)
for /f "delims=" %%F in ('dir /b /s "%OUT%\SDL3.dll" 2^>nul') do (
  if /I not "%%~dpF"=="%OUT%\" copy /y "%%F" "%OUT%\SDL3.dll" >nul
)

if not exist "%OUT%\PcActivityTracker.exe" (
  echo [ERROR] Build completed but PcActivityTracker.exe was not found.
  exit /b 1
)

echo.
echo SUCCESS
echo EXE: %OUT%\PcActivityTracker.exe
exit /b 0
