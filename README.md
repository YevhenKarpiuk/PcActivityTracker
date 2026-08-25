# PcActivityTrackerCpp

Лёгкий перенос `PcActivityTracker` на C++20 без .NET/WPF/Qt.

## Главный приоритет сейчас

1. Корректность общей логики, данных и надёжности.
2. Рабочая сборка Windows x64 из VS Code/CMake без Visual Studio.
3. Linux/macOS остаются архитектурно отделёнными, но их дальнейшее развитие не является текущим приоритетом.

Для Windows используйте [WINDOWS_BUILD.md](WINDOWS_BUILD.md) и `build_windows.cmd`.

## Стек

- C++20
- SDL3 `release-3.4.14`
- Dear ImGui `v1.92.9`
- ImPlot `v1.0`
- SQLite3
- собственный небольшой JSON reader/writer для совместимости `settings.json`
- CMake + Ninja

SDL3/Dear ImGui/ImPlot скачиваются CMake через `FetchContent` при первой полной конфигурации.
SQLite берётся из системы, если найден `find_package(SQLite3)`; иначе `cmake/SQLite3.cmake`
скачивает официальную amalgamation и собирает её статически. В Windows-инструментарии
`C:\tools\mingw64` системного SQLite нет, поэтому используется amalgamation.

## Реализовано

- фоновый мониторинг активности;
- SQLite и миграция старой таблицы `activity_records`;
- crash-checkpoint текущего интервала и восстановление после некорректного завершения;
- повторная запись при временной ошибке SQLite;
- категории и исключения;
- чтение legacy `.NET DateTime.ToString("O")`;
- чтение старых PascalCase-полей `settings.json`, включая `\\uXXXX` Unicode;
- portable-режим через `portable.txt` / `data`;
- Windows active window + idle-time + browser URL через UI Automation;
- автозапуск Windows;
- SDL3 tray;
- вкладки «Сегодня», «История», «Отчёты», «Настройки»;
- экспорт истории CSV/JSON;
- удаление истории по фильтру и полностью;
- универсальный конструктор отчётов;
- столбчатый, линейный и круговой графики;
- промежуточные итоги и общий `ИТОГО` непосредственно в отчёте.

## Универсальные отчёты

Группировки:

- программа;
- категория;
- домен браузера;
- заголовок окна;
- статус «Активность/Простой»;
- день;
- день недели;
- час.

Показатели:

- общее время;
- активное время;
- простой;
- количество периодов;
- средняя длительность;
- минимум;
- максимум;
- доля, %.

Также есть период `с/по`, фильтры, Top N + «Остальные», сортировка, промежуточные итоги, общий `ИТОГО`, CSV/JSON экспорт итоговых строк.

`Среднее/Минимум/Максимум` считаются по исходным периодам активности, а не по техническим кускам, получившимся при разрезании интервала по дням/часам.

## Совместимость БД

Сохраняются старые поля `activity_records`:

```text
id
start_time
end_time
duration_seconds
process_name
window_title
exe_path
browser_url
browser_domain
is_idle
category
```

Для быстрых диапазонных запросов миграция добавляет `start_epoch_ms` и `end_epoch_ms`. Миграция идемпотентна.

`duration_seconds` читается и записывается как 64-битный SQLite `INTEGER`, поэтому импортированные/длительные записи не переполняются через 32-битный `int`.

## Windows

Инструментарий закреплён за `C:\tools`:

```text
C:\tools\mingw64   GCC 14.2.0 (MinGW-w64 UCRT) + windres
C:\tools\cmake     CMake 4.1.2 + ctest
C:\tools\ninja     Ninja 1.13.1
```

Дополнительно нужен Git — им `FetchContent` забирает SDL3/ImGui/ImPlot.

```cmd
build_windows.cmd
```

Скрипт выполняет чистую конфигурацию, Release-сборку, тесты и копирует нужные runtime DLL рядом с:

```text
build\windows-release\PcActivityTracker.exe
```

Подробно: [WINDOWS_BUILD.md](WINDOWS_BUILD.md).

## Быстрые тесты общего кода

Без GUI-зависимостей:

```bash
cmake --preset core-tests-system-sqlite
cmake --build --preset core-tests
ctest --preset core-tests
```

На Windows то же самое одной командой инструментами из `C:\tools`:

```cmd
C:\tools\cmake\bin\cmake.exe --workflow --preset windows-core-tests
```

Последняя проверка: Windows 11 x64, GCC 14.2.0 из `C:\tools\mingw64`, CMake 4.1.2,
Ninja, флаги `-Wall -Wextra -Wpedantic -Werror`:

```text
3/3 tests passed
- core
- reports
- data
```

Полный Windows `.exe` в этой проверке не собирался. Приёмочная проверка GUI-сборки —
`build_windows.cmd` (configure + build + ctest + runtime DLL + `.exe`).

## Portable mode

**По умолчанию боевая база и настройки лежат в каталоге самой программы:**

```text
<каталог с exe>\data\activity_tracker.db
<каталог с exe>\data\settings.json
```

Порядок выбора каталога данных ([AppStoragePaths.cpp](src/data/AppStoragePaths.cpp)):

1. `portable.txt` рядом с `.exe` либо уже существующие `data\activity_tracker.db`/`data\settings.json` —
   безусловный приоритет, чтобы обновление никогда не теряло историю;
2. `<каталог с exe>\data`, если туда физически можно писать — обычный режим;
3. `%LOCALAPPDATA%\PcActivityTracker` — только если каталог программы недоступен на запись
   (установка в `Program Files`, read-only носитель, сетевой ресурс).

Экспорт истории и отчётов (`history.csv/json`, `report.csv/json`) складывается в тот же каталог
данных, а не в текущий каталог процесса; полный путь показывается в статусной строке.

## Структура

```text
src/core       — мониторинг и общая логика
src/data       — SQLite, settings, пути, экспорт
src/reports    — ReportEngine и экспорт отчётов
src/ui         — SDL3 + Dear ImGui + ImPlot
src/platform   — узкие OS-specific backend'ы
resources      — ресурсы executable
assets         — иконки
tests          — core/reports/data тесты
```

## Codex

В проекте есть `AGENTS.md` в корне и локальные инструкции в `src/core`, `src/data`, `src/reports`, `src/ui`, `src/platform`, `tests`.
