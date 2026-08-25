#include "platform/Autostart.h"
#ifdef _WIN32
// libstdc++ на MinGW уже задаёт NOMINMAX=1 из bits/os_defines.h, поэтому определять его
// безусловно нельзя: получается конфликтующее переопределение.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace pcat {
bool setAutostart(bool enabled,const std::filesystem::path& executablePath,std::string& error){
    HKEY key{};const auto open=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);if(open!=ERROR_SUCCESS){error="Cannot open HKCU Run registry key";return false;}
    const wchar_t* name=L"PcActivityTracker";LONG rc=ERROR_SUCCESS;
    if(enabled){std::wstring value=L"\""+executablePath.wstring()+L"\"";rc=RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));}
    else{rc=RegDeleteValueW(key,name);if(rc==ERROR_FILE_NOT_FOUND)rc=ERROR_SUCCESS;}
    RegCloseKey(key);if(rc!=ERROR_SUCCESS){error="Cannot update autostart registry value";return false;}error.clear();return true;
}
}
#endif
