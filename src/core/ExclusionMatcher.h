#pragma once
#include "core/ActivitySnapshot.h"
#include "core/AppSettings.h"
#include <string>

namespace pcat {
bool isBrowserProcess(const std::string& processName);
bool isExcluded(const ActivitySnapshot& snapshot, const AppSettings& settings);
std::string normalizeDomain(std::string domain);
}
