# Каталоги *-subbuild, которые FetchContent создаёт в build/<preset>/_deps, запоминают свой
# абсолютный путь в CMakeCache.txt. После переноса папки проекта CMake отказывается их
# использовать («CMakeCache.txt directory ... is different»), и configure падает.
# Исходники (*-src) при этом остаются годными, поэтому устаревшие subbuild просто удаляются:
# FetchContent пересоздаст их без повторной загрузки.

include_guard(GLOBAL)

if(DEFINED FETCHCONTENT_BASE_DIR)
    set(_pcat_deps_dir "${FETCHCONTENT_BASE_DIR}")
else()
    set(_pcat_deps_dir "${CMAKE_BINARY_DIR}/_deps")
endif()

file(GLOB _pcat_subbuild_caches LIST_DIRECTORIES false "${_pcat_deps_dir}/*-subbuild/CMakeCache.txt")
foreach(_pcat_cache IN LISTS _pcat_subbuild_caches)
    get_filename_component(_pcat_subbuild "${_pcat_cache}" DIRECTORY)
    file(STRINGS "${_pcat_cache}" _pcat_line REGEX "^CMAKE_CACHEFILE_DIR:INTERNAL=" LIMIT_COUNT 1)
    string(REGEX REPLACE "^CMAKE_CACHEFILE_DIR:INTERNAL=" "" _pcat_recorded "${_pcat_line}")
    if(_pcat_recorded STREQUAL "")
        continue()
    endif()
    file(TO_CMAKE_PATH "${_pcat_recorded}" _pcat_recorded)
    set(_pcat_actual "${_pcat_subbuild}")
    if(CMAKE_HOST_WIN32)
        # Пути Windows регистронезависимы: c:/Users и C:/Users — один каталог.
        string(TOLOWER "${_pcat_recorded}" _pcat_recorded)
        string(TOLOWER "${_pcat_actual}" _pcat_actual)
    endif()
    if(NOT _pcat_recorded STREQUAL _pcat_actual)
        message(STATUS "FetchContent: ${_pcat_subbuild} создан в другом каталоге (${_pcat_recorded}), пересоздаю")
        file(REMOVE_RECURSE "${_pcat_subbuild}")
    endif()
endforeach()

unset(_pcat_deps_dir)
unset(_pcat_subbuild_caches)
unset(_pcat_cache)
unset(_pcat_subbuild)
unset(_pcat_line)
unset(_pcat_recorded)
unset(_pcat_actual)
