# PcActivityTrackerCpp — инструкции для Codex

## Цель проекта
Трекер активности ПК на C++20. GUI: SDL3 + Dear ImGui. Графики: ImPlot. База: SQLite. Сборка: CMake + Ninja из VS Code, без обязательной Visual Studio и без .NET/Qt.

Текущий приоритет разработки: (1) все общие ошибки логики/данных/надёжности, (2) рабочая Windows x64 сборка через MinGW-w64 UCRT из `C:\tools`, (3) Linux/macOS после стабилизации Windows. Не откладывай общий дефект только потому, что он не связан с Windows.

## Главные архитектурные правила
1. `src/core`, `src/data` и `src/reports` не должны включать Win32, Cocoa/AppKit, X11, SDL, ImGui или ImPlot.
2. OS API допускаются только в `src/platform/<os>` и, для интеграций рабочего стола, в узких файлах `src/ui/Autostart.*`/`Tray.*`.
3. Не добавляй тяжёлый GUI framework (Qt, wxWidgets, GTK) и новые зависимости без явной необходимости.
4. Любая новая функция отчётов сначала реализуется в `reports`, затем отображается в UI. Не считай агрегаты внутри ImGui-кода.
5. Не превращай platform backend в условные `#ifdef` по всему проекту. CMake выбирает один backend-файл.
6. Не удаляй поддержку `portable.txt` и папки `data` рядом с executable.

## Совместимость данных
Сохраняй существующую таблицу SQLite `activity_records` и поля:
`id, start_time, end_time, duration_seconds, process_name, window_title, exe_path, browser_url, browser_domain, is_idle, category`.

Старая .NET-версия записывала даты через `DateTime.ToString("O")`. C++-парсер обязан принимать эту форму. Миграции должны быть идемпотентными. Сначала добавляй недостающие колонки, потом создавай индексы.

`settings.json` должен продолжать читать старые PascalCase-поля. `StartWithWindows` считается legacy-алиасом `StartWithSystem`.

## Отчёты и итоги
Универсальный отчёт строится только через `ReportEngine`.
- Можно выбирать несколько группировок и показателей.
- Общий `ИТОГО` — агрегат всего отфильтрованного набора, а не сумма процентов строк.
- Промежуточные итоги — агрегаты исходных записей для соответствующего префикса группировок.
- `SharePercent = row.totalSeconds / grandTotal.totalSeconds * 100`.
- Периоды, пересекающие день/час, должны корректно разрезаться для временных группировок.
- Строки итогов должны экспортироваться в CSV/JSON так же, как показываются в таблице.
- При изменении алгоритма агрегации обязательно добавляй/обновляй тест в `tests/test_reports.cpp`.

## Platform backends
Интерфейс — `IActivityProvider`.
- Windows: Win32 допустим только в `src/platform/windows`.
- macOS: AppKit/ApplicationServices допустимы только в `src/platform/macos`.
- Linux: X11/XScreenSaver опциональны. Проект обязан собираться и без них; тогда provider сообщает ограниченные capabilities.
- Native Wayland не предоставляет универсальный API глобального активного окна. Не добавляй compositor-specific обязательную зависимость в core.
- Browser URL — optional capability. Не добавляй COM/UIAutomation в core. Если реализуешь, делай отдельным platform-specific расширением с graceful fallback.

## Сборка
Главная приёмочная сборка на Windows:
```cmd
build_windows.cmd
```
Она обязана завершить configure + build + ctest и создать `build\windows-release\PcActivityTracker.exe`.

Windows presets закреплены за локальным инструментарием:

- `C:\tools\mingw64\bin` — GCC/G++/windres (MinGW-w64 UCRT);
- `C:\tools\cmake\bin` — CMake/CTest;
- `C:\tools\ninja\ninja.exe` — `CMAKE_MAKE_PROGRAM`.

Пути переопределяются переменными `PCAT_TOOLS_ROOT`/`PCAT_MINGW_ROOT`. Не переводить проект на MSVC/Visual Studio без явного запроса.

`build_windows.cmd` обязан оставаться ASCII-only: `cmd.exe` читает `.cmd` в OEM-кодировке, и кириллица внутри батника ломает разбор строк.

SQLite3 подключается через `cmake/SQLite3.cmake`: сначала `find_package(SQLite3)`, при отсутствии — официальная amalgamation через `FetchContent` (`PCAT_SQLITE_MODE`, `PCAT_SQLITE_URL`). Не возвращай безусловный `find_package(SQLite3 REQUIRED)`: в `C:\tools\mingw64` системного SQLite нет.

Общие non-GUI тесты:
```bash
cmake --preset core-tests-system-sqlite
cmake --build --preset core-tests
ctest --preset core-tests
```

Перед завершением изменения:
1. Собери затронутые targets.
2. Запусти `ctest` при изменении core/data/reports.
3. Для изменений Windows/CMake/UI обязательно проверь, что `build_windows.cmd`, `CMakePresets.json` и `WINDOWS_BUILD.md` согласованы. Не заявляй Windows build как проверенный, если `.exe` реально не был собран Windows toolchain.
4. Не коммить `build/`, БД, WAL/SHM и временные настройки.
5. Проверь UTF-8; исходники и Markdown хранятся в UTF-8.

## Стиль C++
- C++20, RAII, `std::filesystem`, `std::chrono`, `std::jthread`.
- Не используй raw owning pointers.
- Ошибки SQLite/файлов должны либо обрабатываться, либо превращаться в понятное исключение на границе операции.
- Фоновый мониторинг не должен падать из-за одиночной ошибки чтения активного окна.
- UI не должен держать mutex БД во время рендеринга кадра.
