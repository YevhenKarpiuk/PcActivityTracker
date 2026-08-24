#pragma once
#include "core/ActivityRecord.h"
#include <filesystem>
#include <vector>
namespace pcat {
void exportActivityCsv(const std::vector<ActivityRecord>& records,const std::filesystem::path& path);
void exportActivityJson(const std::vector<ActivityRecord>& records,const std::filesystem::path& path);
}
