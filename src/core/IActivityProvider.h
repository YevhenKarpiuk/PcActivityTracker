#pragma once
#include "core/ActivitySnapshot.h"
#include <string>

namespace pcat {
struct ProviderCapabilities {
    bool activeWindow{};
    bool idleTime{};
    bool executablePath{};
    bool browserUrl{};
    std::string note;
};

class IActivityProvider {
public:
    virtual ~IActivityProvider() = default;
    virtual ActivitySnapshot capture() = 0;
    virtual ProviderCapabilities capabilities() const = 0;
};
}
