#include "data/ActivityExporter.h"
#include "core/TimeUtil.h"
#include <fstream>
#include <stdexcept>

namespace pcat {
namespace {
std::string csv(const std::string&s){std::string o="\"";for(char c:s){if(c=='\"')o+="\"\"";else o+=c;}return o+"\"";}
std::string json(const std::string&s){static constexpr char h[]="0123456789ABCDEF";std::string o;for(const char raw:s){const auto c=static_cast<unsigned char>(raw);switch(c){case '\\':o+="\\\\";break;case '"':o+="\\\"";break;case '\n':o+="\\n";break;case '\r':o+="\\r";break;case '\t':o+="\\t";break;default:if(c<0x20){o+="\\u00";o+=h[c>>4];o+=h[c&15];}else o+=static_cast<char>(c);}}return o;}
}
void exportActivityCsv(const std::vector<ActivityRecord>& records,const std::filesystem::path& path){std::ofstream f(path,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("Cannot create activity CSV");f<<"\xEF\xBB\xBFstart_time;end_time;duration_seconds;process_name;window_title;exe_path;browser_url;browser_domain;is_idle;category\n";for(const auto&r:records)f<<csv(timeutil::toIso8601Utc(r.startTime))<<';'<<csv(timeutil::toIso8601Utc(r.endTime))<<';'<<r.durationSeconds<<';'<<csv(r.processName)<<';'<<csv(r.windowTitle)<<';'<<csv(r.exePath)<<';'<<csv(r.browserUrl)<<';'<<csv(r.browserDomain)<<';'<<(r.isIdle?1:0)<<';'<<csv(r.category)<<'\n';if(!f)throw std::runtime_error("Failed to write activity CSV");}
void exportActivityJson(const std::vector<ActivityRecord>& records,const std::filesystem::path& path){std::ofstream f(path,std::ios::binary|std::ios::trunc);if(!f)throw std::runtime_error("Cannot create activity JSON");f<<"[\n";for(std::size_t i=0;i<records.size();++i){const auto&r=records[i];f<<"  {\"start_time\":\""<<json(timeutil::toIso8601Utc(r.startTime))<<"\",\"end_time\":\""<<json(timeutil::toIso8601Utc(r.endTime))<<"\",\"duration_seconds\":"<<r.durationSeconds<<",\"process_name\":\""<<json(r.processName)<<"\",\"window_title\":\""<<json(r.windowTitle)<<"\",\"exe_path\":\""<<json(r.exePath)<<"\",\"browser_url\":\""<<json(r.browserUrl)<<"\",\"browser_domain\":\""<<json(r.browserDomain)<<"\",\"is_idle\":"<<(r.isIdle?"true":"false")<<",\"category\":\""<<json(r.category)<<"\"}"<<(i+1<records.size()?",":"")<<'\n';}f<<"]\n";if(!f)throw std::runtime_error("Failed to write activity JSON");}
}
