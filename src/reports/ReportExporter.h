#pragma once
#include "reports/ReportTypes.h"
#include <filesystem>
namespace pcat { void exportReportCsv(const ReportResult&r,const std::filesystem::path&p); void exportReportJson(const ReportResult&r,const std::filesystem::path&p); }
