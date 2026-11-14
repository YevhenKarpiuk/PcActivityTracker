#include "platform/SessionEnd.h"
namespace pcat {
struct SessionEndWatcher::Impl {};
SessionEndWatcher::SessionEndWatcher(void*, std::function<void()>) : impl_(std::make_unique<Impl>()) {}
SessionEndWatcher::~SessionEndWatcher() = default;
}
