# Tests — Codex

Обязательные регрессии:

- мониторинг: временная ошибка repository, повторный flush, pause/stop;
- отчёты: grand total, subtotals, доля %, Top N + «Остальные», totals-only, сортировка, фильтры;
- временные группировки: интервалы через границу часа/дня, при этом Average/Min/Max считаются по исходным периодам;
- SQLite: legacy migration, crash-checkpoint, транзакционное закрытие записи, Unicode path, 64-битная duration;
- settings: PascalCase, `StartWithWindows`, `\\uXXXX`, surrogate pair, BOM, пустые коллекции и ошибочные типы;
- ISO-8601: timezone offsets, дробные секунды, pre-1970 round-trip.

При изменении `core/data/reports` тесты должны проходить с `PCAT_STRICT_WARNINGS=ON`. Для изменений памяти/парсеров по возможности дополнительно запускать ASan/UBSan.
