#include "platform/Autostart.h"
#ifdef __APPLE__
#include <cstdlib>
#include <fstream>
namespace pcat {
namespace {std::string xml(const std::string&s){std::string o;for(char c:s){switch(c){case '&':o+="&amp;";break;case '<':o+="&lt;";break;case '>':o+="&gt;";break;case '"':o+="&quot;";break;case '\'':o+="&apos;";break;default:o+=c;}}return o;}}
bool setAutostart(bool enabled,const std::filesystem::path& executablePath,std::string& error){
    const char* home=std::getenv("HOME");if(!home){error="HOME is not set";return false;}const auto dir=std::filesystem::path(home)/"Library"/"LaunchAgents",file=dir/"com.pcat.PcActivityTracker.plist";std::error_code ec;
    if(!enabled){std::filesystem::remove(file,ec);if(ec){error=ec.message();return false;}error.clear();return true;}
    std::filesystem::create_directories(dir,ec);if(ec){error=ec.message();return false;}std::ofstream out(file,std::ios::binary|std::ios::trunc);if(!out){error="Cannot create LaunchAgent plist";return false;}
    out<<"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\"><dict><key>Label</key><string>com.pcat.PcActivityTracker</string><key>ProgramArguments</key><array><string>"<<xml(executablePath.string())<<"</string></array><key>RunAtLoad</key><true/></dict></plist>\n";out.flush();if(!out){error="Cannot write LaunchAgent plist";return false;}error.clear();return true;
}
}
#endif
