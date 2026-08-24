#include "platform/Autostart.h"
#if defined(__linux__)
#include <cstdlib>
#include <fstream>
namespace pcat {
namespace {
std::string desktopQuote(const std::string& value){std::string out="\"";for(char c:value){if(c=='%'){out+="%%";continue;}if(c=='\\'||c=='\"'||c=='`'||c=='$')out+='\\';out+=c;}return out+'\"';}
}
bool setAutostart(bool enabled,const std::filesystem::path& executablePath,std::string& error){
    std::filesystem::path config;if(const char* xdg=std::getenv("XDG_CONFIG_HOME"))config=xdg;else if(const char* home=std::getenv("HOME"))config=std::filesystem::path(home)/".config";else{error="HOME/XDG_CONFIG_HOME is not set";return false;}
    const auto dir=config/"autostart",file=dir/"PcActivityTracker.desktop";std::error_code ec;
    if(!enabled){std::filesystem::remove(file,ec);if(ec){error=ec.message();return false;}error.clear();return true;}
    std::filesystem::create_directories(dir,ec);if(ec){error=ec.message();return false;}
    std::ofstream out(file,std::ios::binary|std::ios::trunc);if(!out){error="Cannot create autostart .desktop file";return false;}
    out<<"[Desktop Entry]\nType=Application\nName=PcActivityTracker\nExec="<<desktopQuote(executablePath.string())<<"\nTerminal=false\nX-GNOME-Autostart-enabled=true\n";out.flush();if(!out){error="Cannot write autostart .desktop file";return false;}error.clear();return true;
}
}
#endif
