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

SDL3/Dear ImGui/ImPlot скачиваются CMake через `FetchContent` при первой полной конфигурации. SQLite для Windows берётся из MSYS2 UCRT64.

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

Самый простой вариант после установки MSYS2 UCRT64:

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

Последняя проверка в рабочей среде:

```text
3/3 tests passed
- core
- reports
- data
```

Дополнительно общий код прошёл GCC `-Wall -Wextra -Wpedantic -Werror` и ASan/UBSan.

Полный Windows `.exe` в текущей Linux-среде физически не собирался: здесь отсутствуют Windows SDK/MinGW и заблокирована загрузка GUI-зависимостей через `git clone`. Поэтому окончательная Windows-проверка предусмотрена `build_windows.cmd` на вашей Windows-машине.

## Portable mode

Создайте рядом с `.exe`:

```text
portable.txt
```

Данные будут храниться в:

```text
data\activity_tracker.db
data\settings.json
```

Без `portable.txt` Windows использует `%LOCALAPPDATA%\PcActivityTracker`.

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
