cmake_minimum_required(VERSION 3.25)
project(PcActivityTrackerCpp VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PCAT_BUILD_APP "Build the SDL3/ImGui desktop app" ON)
option(PCAT_BUILD_DATA "Build SQLite/settings data layer" ON)
option(PCAT_BUILD_TESTS "Build unit tests" ON)
option(PCAT_FETCH_DEPS "Fetch GUI dependencies with CMake" ON)
option(PCAT_STRICT_WARNINGS "Treat warnings in PcActivityTracker sources as errors" OFF)

# Тесты построены на assert(), а Release определяет NDEBUG и вырезает их целиком.
# Без этого ctest в Release-пресете рапортует об успехе, фактически ничего не проверив.
function(pcat_enable_assertions target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${target} PRIVATE -UNDEBUG)
    endif()
endfunction()

function(pcat_target_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(PCAT_STRICT_WARNINGS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(PCAT_STRICT_WARNINGS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(pcat_core
    src/core/ActivityMonitor.cpp
    src/core/BrowserAddress.cpp
    src/core/CategoryResolver.cpp
    src/core/ExclusionMatcher.cpp
    src/core/TimeUtil.cpp
    src/core/TextUtil.cpp
)
target_include_directories(pcat_core PUBLIC src)
target_compile_features(pcat_core PUBLIC cxx_std_20)
pcat_target_warnings(pcat_core)

add_library(pcat_reports
    src/reports/ReportEngine.cpp
    src/reports/ReportExporter.cpp
)
target_include_directories(pcat_reports PUBLIC src)
target_link_libraries(pcat_reports PUBLIC pcat_core)
pcat_target_warnings(pcat_reports)

if(PCAT_BUILD_DATA)
    include(cmake/SQLite3.cmake)
    add_library(pcat_data
        src/data/AppStoragePaths.cpp
        src/data/ActivityRepository.cpp
        src/data/ActivityExporter.cpp
        src/data/SettingsService.cpp
    )
    target_include_directories(pcat_data PUBLIC src)
    target_link_libraries(pcat_data PUBLIC pcat_core SQLite::SQLite3)
    pcat_target_warnings(pcat_data)
endif()

if(PCAT_BUILD_APP)
    if(NOT TARGET pcat_data)
        message(FATAL_ERROR "PCAT_BUILD_APP requires PCAT_BUILD_DATA=ON")
    endif()
    include(cmake/Dependencies.cmake)

    if(WIN32)
        set(PCAT_PLATFORM_SRC src/platform/windows/WindowsActivityProvider.cpp src/platform/windows/Autostart.cpp)
    elseif(APPLE)
        enable_language(OBJCXX)
        set(PCAT_PLATFORM_SRC src/platform/macos/MacActivityProvider.mm src/platform/macos/Autostart.cpp)
    elseif(UNIX)
        set(PCAT_PLATFORM_SRC src/platform/linux/LinuxActivityProvider.cpp src/platform/linux/Autostart.cpp)
    else()
        set(PCAT_PLATFORM_SRC src/platform/common/NullActivityProvider.cpp src/platform/common/Autostart.cpp)
    endif()

    add_library(pcat_platform ${PCAT_PLATFORM_SRC})
    target_include_directories(pcat_platform PUBLIC src)
    target_link_libraries(pcat_platform PUBLIC pcat_core)
    if(WIN32)
        target_compile_definitions(pcat_platform PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00 UNICODE _UNICODE)
        target_link_libraries(pcat_platform PRIVATE user32 psapi advapi32 ole32 oleaut32 uuid)
    elseif(APPLE)
        find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
        find_library(APPSERVICES_FRAMEWORK ApplicationServices REQUIRED)
        target_link_libraries(pcat_platform PRIVATE ${APPKIT_FRAMEWORK} ${APPSERVICES_FRAMEWORK})
    elseif(UNIX)
        find_package(X11 QUIET)
        if(X11_FOUND)
            target_compile_definitions(pcat_platform PRIVATE PCAT_HAVE_X11=1)
            target_include_directories(pcat_platform PRIVATE ${X11_INCLUDE_DIR})
            target_link_libraries(pcat_platform PRIVATE ${X11_LIBRARIES})
            if(X11_Xss_FOUND)
                target_compile_definitions(pcat_platform PRIVATE PCAT_HAVE_XSS=1)
                target_link_libraries(pcat_platform PRIVATE ${X11_Xss_LIB})
            endif()
        endif()
    endif()
    pcat_target_warnings(pcat_platform)

    set(PCAT_APP_SOURCES
        src/main.cpp
        src/ui/App.cpp
        src/ui/ReportView.cpp
        src/ui/TrayIcon.cpp
    )
    # Единственный источник правды для идентификатора иконки: подставляется и в .rc,
    # и в C++, который забирает её через LoadImage для окна и трея.
    set(PCAT_ICON_RESOURCE_ID 101)
    if(WIN32)
        enable_language(RC)
        file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/PcActivityTracker.ico" PCAT_ICON_PATH)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/PcActivityTracker.rc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/PcActivityTracker.rc"
            @ONLY
        )
        list(APPEND PCAT_APP_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/PcActivityTracker.rc")
    endif()
    add_executable(PcActivityTracker ${PCAT_APP_SOURCES})
    target_include_directories(PcActivityTracker PRIVATE src)
    target_link_libraries(PcActivityTracker PRIVATE
        pcat_core pcat_reports pcat_data pcat_platform
        SDL3::SDL3 imgui implot
    )
    if(WIN32)
        set_target_properties(PcActivityTracker PROPERTIES WIN32_EXECUTABLE TRUE)
        target_compile_definitions(PcActivityTracker PRIVATE PCAT_ICON_RESOURCE_ID=${PCAT_ICON_RESOURCE_ID})
        # user32/gdi32 нужны TrayIcon.cpp для LoadImage/GetIconInfo/GetDIBits.
        target_link_libraries(PcActivityTracker PRIVATE user32 gdi32)
    endif()
    pcat_target_warnings(PcActivityTracker)
endif()

if(PCAT_BUILD_TESTS)
    enable_testing()
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE pcat_core)
    pcat_target_warnings(test_core)
    pcat_enable_assertions(test_core)
    add_test(NAME core COMMAND test_core)
    add_executable(test_reports tests/test_reports.cpp)
    target_link_libraries(test_reports PRIVATE pcat_reports)
    pcat_target_warnings(test_reports)
    pcat_enable_assertions(test_reports)
    add_test(NAME reports COMMAND test_reports)
    if(TARGET pcat_data)
        add_executable(test_data tests/test_data.cpp)
        target_link_libraries(test_data PRIVATE pcat_data)
        pcat_target_warnings(test_data)
        pcat_enable_assertions(test_data)
        add_test(NAME data COMMAND test_data)
    endif()
endif()
