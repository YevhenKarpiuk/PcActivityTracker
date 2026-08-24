@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "MSYS2_ROOT=C:\msys64"
if not exist "%MSYS2_ROOT%\ucrt64\bin\g++.exe" (
  echo [ERROR] MSYS2 UCRT64 compiler not found: %MSYS2_ROOT%\ucrt64\bin\g++.exe
  echo Install packages listed in WINDOWS_BUILD.md first.
  exit /b 1
)

set "PATH=%MSYS2_ROOT%\ucrt64\bin;%MSYS2_ROOT%\usr\bin;%PATH%"

for %%T in (cmake.exe ninja.exe gcc.exe g++.exe windres.exe git.exe) do (
  where %%T >nul 2>nul || (
    echo [ERROR] %%T not found in PATH.
    exit /b 1
  )
)

if not exist "%MSYS2_ROOT%\ucrt64\include\sqlite3.h" (
  echo [ERROR] SQLite3 development package is not installed.
  echo Run: pacman -S --needed mingw-w64-ucrt-x86_64-sqlite3
  exit /b 1
)

echo [1/4] Configure Windows Release...
cmake --preset windows-release --fresh || exit /b 1

echo [2/4] Build...
cmake --build --preset windows-release || exit /b 1

echo [3/4] Tests...
ctest --preset windows-release || exit /b 1

echo [4/4] Copy runtime DLLs next to executable...
set "OUT=%~dp0build\windows-release"
for %%D in (libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libsqlite3-0.dll) do (
  if exist "%MSYS2_ROOT%\ucrt64\bin\%%D" copy /y "%MSYS2_ROOT%\ucrt64\bin\%%D" "%OUT%\%%D" >nul
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
