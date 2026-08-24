# Windows x64: сборка в VS Code без Visual Studio

Основная целевая сборка проекта сейчас — **Windows x64, MSYS2 UCRT64 + GCC + Ninja + CMake**.

Visual Studio, MSVC, .NET и Qt не требуются.

## 1. Установить MSYS2

Установите MSYS2 в стандартный каталог:

```text
C:\msys64
```

Откройте **MSYS2 UCRT64** и сначала обновите систему:

```bash
pacman -Syu
```

Если будет предложено закрыть терминал, снова откройте **MSYS2 UCRT64**, затем установите:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-sqlite3 \
  git
```

Проверка:

```bash
g++ --version
cmake --version
ninja --version
windres --version
```

## 2. VS Code

Откройте корневую папку `PcActivityTrackerCpp`.

Рекомендуемые расширения уже указаны в `.vscode/extensions.json`:

- C/C++ (`ms-vscode.cpptools`)
- CMake Tools (`ms-vscode.cmake-tools`)

Visual Studio Build Tools устанавливать не нужно.

## 3. Рекомендуемая сборка

В обычном терминале VS Code (`cmd.exe`) из корня проекта:

```cmd
build_windows.cmd
```

Скрипт:

1. использует `C:\msys64\ucrt64`;
2. проверяет GCC, G++, `windres`, CMake, Ninja, Git и SQLite headers;
3. делает **чистую** конфигурацию `cmake --preset windows-release --fresh`;
4. собирает приложение;
5. выполняет `ctest`;
6. копирует DLL GCC/SQLite/SDL3 рядом с executable;
7. проверяет наличие `PcActivityTracker.exe`.

Результат:

```text
build\windows-release\PcActivityTracker.exe
```

Запуск:

```cmd
run_windows.cmd
```

## 4. Команды вручную

Windows presets явно закреплены за UCRT64-компиляторами:

```text
C:\msys64\ucrt64\bin\gcc.exe
C:\msys64\ucrt64\bin\g++.exe
C:\msys64\ucrt64\bin\windres.exe
```

Поэтому из терминала, где доступны CMake/Ninja/Git:

```cmd
cmake --preset windows-release --fresh
cmake --build --preset windows-release
ctest --preset windows-release
```

Debug:

```cmd
cmake --preset windows-debug --fresh
cmake --build --preset windows-debug
ctest --preset windows-debug
```

## 5. Первая конфигурация требует интернет

CMake скачивает зафиксированные версии с GitHub:

- SDL3 `release-3.4.14`
- Dear ImGui `v1.92.9`
- ImPlot `v1.0`

После заполнения `build/_deps` повторная сборка использует уже скачанные исходники.

## 6. Windows-specific части

Только backend Windows использует системные API:

- активное окно: Win32;
- idle time: `GetLastInputInfo`;
- browser URL: Windows UI Automation/COM;
- автозапуск: `HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run`.

В CMake для Windows подключены `user32`, `psapi`, `advapi32`, `ole32`, `oleaut32`, `uuid`.

Иконка и версия приложения встраиваются в `.exe` через Windows resource compiler (`windres`).

## 7. Portable mode

Рядом с `PcActivityTracker.exe` создайте:

```text
portable.txt
```

Тогда:

```text
data\activity_tracker.db
data\settings.json
```

Без portable mode используется:

```text
%LOCALAPPDATA%\PcActivityTracker
```

## 8. Если сборка не прошла

Запустите:

```cmd
build_windows.cmd > windows_build.log 2>&1
```

Пришлите `windows_build.log` целиком. По нему можно будет исправить уже фактическую Windows compiler/linker ошибку, если она останется.
