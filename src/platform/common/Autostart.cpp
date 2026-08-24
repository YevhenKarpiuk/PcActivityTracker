#include "platform/Autostart.h"
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)
namespace pcat { bool setAutostart(bool,const std::filesystem::path&,std::string& error){error="Autostart is not supported on this platform";return false;} }
#endif
