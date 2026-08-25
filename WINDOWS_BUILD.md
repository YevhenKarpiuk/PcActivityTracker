# Windows x64: сборка в VS Code без Visual Studio

Основная целевая сборка проекта сейчас — **Windows x64, MinGW-w64 UCRT GCC + Ninja + CMake из `C:\tools`**.

Visual Studio, MSVC, MSYS2, .NET и Qt не требуются.

## 1. Требуемый инструментарий

Проект закреплён за локальным инструментарием в `C:\tools`:

```text
C:\tools\mingw64\bin\gcc.exe        GCC 14.2.0 (MinGW-w64 UCRT, winlibs)
C:\tools\mingw64\bin\g++.exe
C:\tools\mingw64\bin\windres.exe    Windows resource compiler
C:\tools\cmake\bin\cmake.exe        CMake 4.1.2
C:\tools\cmake\bin\ctest.exe
C:\tools\ninja\ninja.exe            Ninja 1.13.1
```

Дополнительно нужен **Git** (`C:\Program Files\Git\cmd\git.exe`) — CMake `FetchContent`
скачивает им SDL3/Dear ImGui/ImPlot.

Проверка:

```cmd
C:\tools\mingw64\bin\g++.exe --version
C:\tools\cmake\bin\cmake.exe --version
C:\tools\ninja\ninja.exe --version
C:\tools\mingw64\bin\windres.exe --version
git --version
```

Переопределение расположения инструментов (если каталог другой):

```cmd
set PCAT_TOOLS_ROOT=D:\tools
set PCAT_MINGW_ROOT=D:\tools\mingw64
```

### SQLite3

Toolchain `C:\tools\mingw64` **не содержит** SQLite3, поэтому `cmake\SQLite3.cmake` работает так:

1. пробует системную библиотеку через `find_package(SQLite3)` (MSYS2, vcpkg, Linux-дистрибутивы);
2. если её нет — скачивает официальную amalgamation `sqlite-amalgamation-3460100.zip`
   и компилирует статическую библиотеку `SQLite::SQLite3` внутри проекта.

Отдельно ставить SQLite не нужно. Управление:

```cmd
cmake --preset windows-release -DPCAT_SQLITE_MODE=FETCH
cmake --preset windows-release -DPCAT_SQLITE_MODE=SYSTEM
cmake --preset windows-release -DPCAT_SQLITE_URL=C:/offline/sqlite-amalgamation-3460100.zip
```

## 2. VS Code

Откройте корневую папку `PcActivityTrackerCpp`.

Рекомендуемые расширения уже указаны в `.vscode/extensions.json`:

- C/C++ (`ms-vscode.cpptools`)
- CMake Tools (`ms-vscode.cmake-tools`)

`.vscode/settings.json` уже указывает CMake Tools и IntelliSense на `C:\tools`.
Visual Studio Build Tools устанавливать не нужно.

## 3. Рекомендуемая сборка

В обычном терминале VS Code (`cmd.exe`) из корня проекта:

```cmd
build_windows.cmd
```

Скрипт:

1. находит toolchain в `%PCAT_MINGW_ROOT%` (по умолчанию `C:\tools\mingw64`);
2. выбирает CMake/Ninja: сначала `C:\tools\cmake` и `C:\tools\ninja`, затем копии внутри
   toolchain, затем `PATH`;
3. проверяет наличие Git для `FetchContent`;
4. делает **чистую** конфигурацию `cmake --preset windows-release --fresh`;
5. собирает приложение;
6. выполняет `ctest`;
7. копирует runtime DLL (GCC + SDL3) рядом с executable;
8. проверяет наличие `PcActivityTracker.exe`.

Повторная сборка без полной переконфигурации:

```cmd
build_windows.cmd incremental
```

Результат:

```text
build\windows-release\PcActivityTracker.exe
```

Запуск:

```cmd
run_windows.cmd
```

## 4. Команды вручную

Windows presets явно закреплены за компиляторами из `C:\tools\mingw64`:

```text
C:\tools\mingw64\bin\gcc.exe
C:\tools\mingw64\bin\g++.exe
C:\tools\mingw64\bin\windres.exe
```

Ninja берётся из `C:\tools\ninja\ninja.exe` через `CMAKE_MAKE_PROGRAM`, поэтому его
достаточно не иметь в `PATH`.

```cmd
C:\tools\cmake\bin\cmake.exe --preset windows-release --fresh
C:\tools\cmake\bin\cmake.exe --build --preset windows-release
C:\tools\cmake\bin\ctest.exe --preset windows-release
```

Debug:

```cmd
C:\tools\cmake\bin\cmake.exe --preset windows-debug --fresh
C:\tools\cmake\bin\cmake.exe --build --preset windows-debug
C:\tools\cmake\bin\ctest.exe --preset windows-debug
```

Одной командой (workflow preset, CMake 3.25+):

```cmd
C:\tools\cmake\bin\cmake.exe --workflow --preset windows-release
```

Быстрая проверка core/data/reports без GUI-зависимостей и без интернета:

```cmd
C:\tools\cmake\bin\cmake.exe --workflow --preset windows-core-tests
```

Этот preset собирается с `-Wall -Wextra -Wpedantic -Werror`.

## 5. Первая конфигурация требует интернет

CMake скачивает зафиксированные версии:

- SDL3 `release-3.4.14` (GitHub, git)
- Dear ImGui `v1.92.9` (GitHub, git)
- ImPlot `v1.0` (GitHub, git)
- SQLite amalgamation `3.46.1` (sqlite.org, http) — только если системного SQLite3 нет

После заполнения `build/_deps` повторная сборка использует уже скачанные исходники.
`windows-core-tests` не требует GitHub: `PCAT_FETCH_DEPS=OFF`, нужен только архив SQLite.

## 6. Windows-specific части

Только backend Windows использует системные API:

- активное окно: Win32;
- idle time: `GetLastInputInfo`;
- browser URL: Windows UI Automation/COM;
- автозапуск: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.

В CMake для Windows подключены `user32`, `psapi`, `advapi32`, `ole32`, `oleaut32`, `uuid`.
Заголовки `uiautomation.h` и GUID `CLSID_CUIAutomation`/`IID_IUIAutomation` присутствуют
в MinGW-w64 12.0.0 из `C:\tools\mingw64`, отдельный Windows SDK не нужен.

Иконка и версия приложения встраиваются в `.exe` через Windows resource compiler (`windres`).

## 7. Где лежат данные

По умолчанию — в каталоге самой программы:

```text
<каталог с exe>\data\activity_tracker.db
<каталог с exe>\data\settings.json
```

Порядок выбора:

1. `portable.txt` рядом с `.exe` или уже существующие данные в `data\` — безусловный приоритет;
2. `<каталог с exe>\data`, если каталог доступен на запись;
3. `%LOCALAPPDATA%\PcActivityTracker` — только когда писать рядом с программой нельзя
   (`Program Files`, read-only носитель, сетевой ресурс).

Portable-архив содержит `portable.txt`, поэтому режим в нём зафиксирован явно и не зависит
от прав на каталог.

## 8. Если сборка не прошла

Запустите:

```cmd
build_windows.cmd > windows_build.log 2>&1
```

Пришлите `windows_build.log` целиком.

Частые причины:

- `git.exe not found in PATH` — установите Git for Windows;
- ошибка загрузки SQLite amalgamation — используйте
  `-DPCAT_SQLITE_URL=<локальный zip>` или `-DPCAT_SQLITE_MODE=SYSTEM`;
- `build_windows.cmd` должен оставаться в ASCII: `cmd.exe` читает `.cmd` в OEM-кодировке,
  и кириллица внутри батника ломает разбор строк.
