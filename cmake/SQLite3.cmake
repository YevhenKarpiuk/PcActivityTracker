# SQLite3 для PcActivityTracker.
#
# Порядок выбора источника:
#   1. SYSTEM  — библиотека toolchain/дистрибутива через find_package(SQLite3).
#   2. FETCH   — официальная amalgamation, компилируется в статическую библиотеку.
#   AUTO (по умолчанию) сначала пробует SYSTEM, затем FETCH.
#
# Toolchain winlibs/MinGW-w64 в C:\tools\mingw64 не содержит SQLite, поэтому на Windows
# обычно используется amalgamation. В MSYS2/Linux найдётся системная библиотека.

include_guard(GLOBAL)
include(FetchContent)

set(PCAT_SQLITE_MODE "AUTO" CACHE STRING "Источник SQLite3: AUTO, SYSTEM или FETCH")
set_property(CACHE PCAT_SQLITE_MODE PROPERTY STRINGS AUTO SYSTEM FETCH)
set(PCAT_SQLITE_URL "https://www.sqlite.org/2024/sqlite-amalgamation-3460100.zip"
    CACHE STRING "URL или локальный путь к архиву sqlite amalgamation")
set(PCAT_SQLITE_SHA256 "77823cb110929c2bcb0f5d48e4833b5c59a8a6e40cdea3936b99e199dbbe5784"
    CACHE STRING "SHA256 архива sqlite amalgamation")

if(NOT TARGET SQLite::SQLite3)
    if(NOT PCAT_SQLITE_MODE STREQUAL "FETCH")
        if(PCAT_SQLITE_MODE STREQUAL "SYSTEM")
            find_package(SQLite3 REQUIRED)
        else()
            find_package(SQLite3 QUIET)
        endif()
    endif()

    if(TARGET SQLite::SQLite3)
        message(STATUS "SQLite3: системная библиотека ${SQLite3_VERSION}")
    else()
        message(STATUS "SQLite3: системная библиотека не найдена, используется amalgamation ${PCAT_SQLITE_URL}")
        FetchContent_Declare(
            sqlite3_amalgamation
            URL "${PCAT_SQLITE_URL}"
            URL_HASH "SHA256=${PCAT_SQLITE_SHA256}"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(sqlite3_amalgamation)

        if(NOT EXISTS "${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c")
            message(FATAL_ERROR "sqlite3.c не найден в ${sqlite3_amalgamation_SOURCE_DIR}")
        endif()

        add_library(pcat_sqlite3 STATIC "${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c")
        target_include_directories(pcat_sqlite3 SYSTEM PUBLIC "${sqlite3_amalgamation_SOURCE_DIR}")
        # FULLMUTEX в ActivityRepository требует SQLITE_THREADSAFE=1.
        # Загрузка расширений не используется, поэтому её отключение убирает зависимость от libdl.
        target_compile_definitions(pcat_sqlite3 PUBLIC
            SQLITE_THREADSAFE=1
            SQLITE_OMIT_LOAD_EXTENSION=1
            SQLITE_DQS=0
            SQLITE_DEFAULT_MEMSTATUS=0
            SQLITE_DEFAULT_FOREIGN_KEYS=1
            SQLITE_ENABLE_COLUMN_METADATA=1
        )
        set_target_properties(pcat_sqlite3 PROPERTIES C_STANDARD 11 POSITION_INDEPENDENT_CODE ON)
        if(NOT MSVC)
            target_compile_options(pcat_sqlite3 PRIVATE -w)
        endif()
        if(UNIX)
            find_package(Threads REQUIRED)
            target_link_libraries(pcat_sqlite3 PUBLIC Threads::Threads)
            if(NOT APPLE)
                target_link_libraries(pcat_sqlite3 PUBLIC m)
            endif()
        endif()
        add_library(SQLite::SQLite3 ALIAS pcat_sqlite3)
    endif()
endif()
