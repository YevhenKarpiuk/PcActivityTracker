#include "platform/Autostart.h"
// Используется только на платформах без собственного backend (см. CMakeLists.txt).
namespace pcat { AutostartResult setAutostart(bool enabled,const std::filesystem::path&,AutostartReason){if(!enabled)return {};return {false,"Autostart is not supported on this platform",{}};} }
