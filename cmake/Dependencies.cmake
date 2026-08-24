include(FetchContent)

if(NOT PCAT_FETCH_DEPS)
    find_package(SDL3 CONFIG REQUIRED)
    # imgui/implot targets must be supplied by the caller when fetching is disabled.
    return()
endif()

set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.14
    GIT_SHALLOW TRUE
)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)


FetchContent_Declare(
    imgui_src
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui_src)
if(NOT imgui_src_POPULATED)
    FetchContent_Populate(imgui_src)
endif()
add_library(imgui STATIC
    ${imgui_src_SOURCE_DIR}/imgui.cpp
    ${imgui_src_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_src_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_src_SOURCE_DIR} ${imgui_src_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

FetchContent_Declare(
    implot_src
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v1.0
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(implot_src)
if(NOT implot_src_POPULATED)
    FetchContent_Populate(implot_src)
endif()
add_library(implot STATIC
    ${implot_src_SOURCE_DIR}/implot.cpp
    ${implot_src_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot PUBLIC ${implot_src_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui)
