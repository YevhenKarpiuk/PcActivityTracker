# PcActivityTracker

Локальное Windows-приложение для трекинга активности на ПК:
фиксирует активное окно, процесс, длительность сессий и периоды простоя.
Данные сохраняются только локально в SQLite.

## Используемые технологии

- C# / .NET 10
- WPF (`net10.0-windows10.0.19041.0`)
- SQLite (`Microsoft.Data.Sqlite`)
- LiveCharts2 (`LiveChartsCore.SkiaSharpView.WPF`)
- Windows API / UI Automation (получение активного окна, idle-time, домена/URL браузера)

## Требования

- Windows 10/11
- .NET SDK 10.x (ориентир: `global.json`)
- PowerShell 5+ или PowerShell 7+

## Установка

```powershell
git clone https://github.com/<your-account>/PcActivityTracker.git
cd PcActivityTracker
```

Восстановление зависимостей:

```powershell
./restore.ps1
```

Альтернатива:

```powershell
dotnet restore ./src/PcActivityTracker.App/PcActivityTracker.App.csproj
```

## Запуск

Через скрипт:

```powershell
./run.ps1
```

Или вручную:

```powershell
dotnet build ./src/PcActivityTracker.App/PcActivityTracker.App.csproj
dotnet run --project ./src/PcActivityTracker.App/PcActivityTracker.App.csproj --no-build
```

## Сборка

Общая сборка solution:

```powershell
./build.ps1 -Configuration Release
```

Публикация под `win-x64`:

```powershell
./publish-win-x64.ps1 -Configuration Release
```

## Структура проекта

```text
PcActivityTracker/
├─ src/
│  ├─ PcActivityTracker.App/       # WPF UI, tray, view models
│  ├─ PcActivityTracker.Core/      # доменные модели и бизнес-логика
│  ├─ PcActivityTracker.Data/      # SQLite, settings/storage
│  └─ PcActivityTracker.Windows/   # Windows-специфичные провайдеры
├─ scripts/                        # вспомогательные скрипты
├─ build.ps1
├─ restore.ps1
├─ run.ps1
├─ publish-win-x64.ps1
└─ PcActivityTracker.sln
```

## Где хранятся локальные данные

```text
%LOCALAPPDATA%\PcActivityTracker\activity_tracker.db
%LOCALAPPDATA%\PcActivityTracker\settings.json
```

## Примечание по приватности

Приложение не отправляет данные в сеть и не записывает нажатия клавиш.
Собранная активность предназначена для локального использования.
