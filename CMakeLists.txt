cmake_minimum_required(VERSION 3.21)

# Force static linking for all vcpkg dependencies
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)

project(
    STFU
    VERSION 1.2.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(GNUInstallDirs)

# Install into the STFU mod folder (the repo root)
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}" CACHE PATH "Install prefix" FORCE)

# ---- Add CommonLibSSE-NG ----
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/commonlibsse-ng ${CMAKE_CURRENT_BINARY_DIR}/CommonLibSSE-NG EXCLUDE_FROM_ALL)
include(lib/commonlibsse-ng/cmake/CommonLibSSE.cmake)

# ---- Add Detours library ----
add_library(Detours STATIC
    lib/detours/src/creatwth.cpp
    lib/detours/src/detours.cpp
    lib/detours/src/disasm.cpp
    lib/detours/src/disolarm.cpp
    lib/detours/src/disolarm64.cpp
    lib/detours/src/disolia64.cpp
    lib/detours/src/disolx64.cpp
    lib/detours/src/disolx86.cpp
    lib/detours/src/image.cpp
    lib/detours/src/modules.cpp
)
target_include_directories(Detours PUBLIC lib/detours/src)
set_target_properties(Detours PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# ---- Add yaml-cpp dependency ----
find_package(yaml-cpp CONFIG REQUIRED)

# ---- Add SQLite3 dependency ----
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# ---- Plugin Target ----
add_commonlibsse_plugin(
    ${PROJECT_NAME}
    NAME "STFU"
    AUTHOR "Zevick"
    USE_ADDRESS_LIBRARY
    SOURCES
        src/main.cpp
        src/Config.cpp
        src/Config_Scenes.cpp
        src/Config_Blocking.cpp
        src/Config_Yaml.cpp
        src/PopulateTopicInfoHook.cpp
        src/ConstructResponseHook.cpp
        src/SceneHook.cpp
        src/SceneMonitor.cpp
        src/DialogueDatabase.cpp
        src/DialogueDatabase_History.cpp
        src/DialogueDatabase_Blacklist.cpp
        src/DialogueDatabase_Whitelist.cpp
        src/DialogueDatabase_Query.cpp
        src/DialogueDatabase_Json.cpp
        src/DialogueLogger.cpp
        src/FormKey.cpp
        src/TopicResponseExtractor.cpp
        src/PrismaUIMenu.cpp
        src/PrismaUIMenu_History.cpp
        src/PrismaUIMenu_Blacklist.cpp
        src/PrismaUIMenu_Whitelist.cpp
        src/PrismaUIMenu_Entries.cpp
        src/PrismaUIMenu_Settings.cpp
        src/SettingsPersistence.cpp
        src/PapyrusInterface.cpp
)

target_compile_features(
    ${PROJECT_NAME}
    PRIVATE
        cxx_std_23
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        yaml-cpp::yaml-cpp
        unofficial::sqlite3::sqlite3
        Detours
)

# Enable PCH - the auto-generated plugin file needs std:: headers
target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
        include/PCH.h
)

# ---- Installation ----
install(
    FILES
        $<TARGET_FILE:${PROJECT_NAME}>
    DESTINATION
        "SKSE/Plugins"
)

install(
    FILES
        $<TARGET_PDB_FILE:${PROJECT_NAME}>
    DESTINATION
        "SKSE/Plugins"
    OPTIONAL
)
