#pragma once
#include <filesystem>
#include <memory>

namespace pcat {
// Не даёт двум процессам вести мониторинг в одну базу: при автозапуске и ручном запуске
// они писали бы одни и те же интервалы дважды. Блокировка привязана к каталогу данных,
// поэтому разные portable-копии со своими базами по-прежнему могут работать одновременно.
class SingleInstance {
public:
    explicit SingleInstance(const std::filesystem::path& dataDirectory);
    ~SingleInstance();
    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    // true — этот процесс первый и должен работать.
    bool isPrimary() const noexcept;
    // Второй процесс просит первый показать окно (например, пользователь запустил ярлык,
    // не заметив, что программа уже работает в трее).
    void requestShowPrimary() noexcept;
    // Первый процесс: была ли такая просьба с прошлого вызова. Дешёвая, без ожидания.
    bool consumeShowRequest() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
