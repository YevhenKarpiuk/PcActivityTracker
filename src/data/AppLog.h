#pragma once
// Простой файловый журнал в каталоге данных. Нужен прежде всего для запуска из автозагрузки:
// там всплывающее сообщение об ошибке легко пропустить, а причину сбоя потом не восстановить.
#include <cstdint>
#include <filesystem>
#include <string_view>

namespace pcat::applog {
enum class Level { Info, Warning, Error };

// Открывает журнал (дописывание). Файл больше maxBytes при открытии переименовывается в *.1,
// так что на диске не больше двух файлов. Повторный вызов переключает файл.
void open(const std::filesystem::path& file, std::uintmax_t maxBytes = 1024 * 1024);
void close() noexcept;
bool isOpen() noexcept;

// Потокобезопасно; ошибки записи проглатываются: журнал не должен ронять программу.
void write(Level level, std::string_view message) noexcept;
inline void info(std::string_view message) noexcept { write(Level::Info, message); }
inline void warning(std::string_view message) noexcept { write(Level::Warning, message); }
inline void error(std::string_view message) noexcept { write(Level::Error, message); }
}
