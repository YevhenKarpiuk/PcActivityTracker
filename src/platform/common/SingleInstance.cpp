#include "platform/SingleInstance.h"
// Платформы без собственной реализации: ограничение одного экземпляра пока не действует.
namespace pcat {
struct SingleInstance::Impl {};
SingleInstance::SingleInstance(const std::filesystem::path&) : impl_(std::make_unique<Impl>()) {}
SingleInstance::~SingleInstance() = default;
bool SingleInstance::isPrimary() const noexcept { return true; }
void SingleInstance::requestShowPrimary() noexcept {}
bool SingleInstance::consumeShowRequest() noexcept { return false; }
}
