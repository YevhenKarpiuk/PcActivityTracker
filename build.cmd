@echo off
setlocal
cmake --preset ninja-release || exit /b 1
cmake --build --preset release || exit /b 1
endlocal
