# Tests — Codex

Обязательные регрессии:

- мониторинг: временная ошибка repository, повторный flush, pause/stop;
- отчёты: grand total, subtotals, доля %, Top N + «Остальные», totals-only, сортировка, фильтры;
- временные группировки: интервалы через границу часа/дня, при этом Average/Min/Max считаются по исходным периодам;
- SQLite: legacy migration, crash-checkpoint, транзакционное закрытие записи, Unicode path, 64-битная duration;
- settings: PascalCase, `StartWithWindows`, `\\uXXXX`, surrogate pair, BOM, пустые коллекции и ошибочные типы;
- ISO-8601: timezone offsets, дробные секунды, pre-1970 round-trip.

Тесты построены на `assert()`, поэтому тестовые цели собираются с `-UNDEBUG` (`pcat_enable_assertions`). Не убирай это: в Release определён `NDEBUG`, и без `-UNDEBUG` `ctest` рапортует об успехе, не выполнив ни одной проверки.

Фикстуры для группировок День/Час/День недели задавай локальным временем через `fromLocalTm`, а не строками с фиксированным смещением: иначе тест ломается на машинах со смещением, не кратным часу.

При изменении `core/data/reports` тесты должны проходить с `PCAT_STRICT_WARNINGS=ON`. Для изменений памяти/парсеров по возможности дополнительно запускать ASan/UBSan.
