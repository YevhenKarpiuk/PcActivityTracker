#pragma once
#include "core/ActivityRecord.h"
#include "reports/ReportTypes.h"
#include <vector>
namespace pcat { ReportResult buildReport(const std::vector<ActivityRecord>& records,const ReportDefinition& definition); }
