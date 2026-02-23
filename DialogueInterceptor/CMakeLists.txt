cmake_minimum_required(VERSION 3.21)

# Force static linking for all vcpkg dependencies
set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)

project(
    STFU
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(GNUInstallDirs)

# Output to parent STFU mod folder (not DialogueInterceptor subfolder)
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/.." CACHE PATH "Install prefix" FORCE)

# ---- Add CommonLibSSE-NG ----
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(RAPIDCSV_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/rapidcsv/src" CACHE PATH "" FORCE)

# Build from source (same as Physical Diaries - this is known to work)
message(STATUS "Building CommonLibSSE from source")
add_subdirectory(external/CommonLibSSE-NG EXCLUDE_FROM_ALL)

# ---- Add Detours library ----
add_library(Detours STATIC
    external/Detours/src/creatwth.cpp
    external/Detours/src/detours.cpp
    external/Detours/src/disasm.cpp
    external/Detours/src/disolarm.cpp
    external/Detours/src/disolarm64.cpp
    external/Detours/src/disolia64.cpp
    external/Detours/src/disolx64.cpp
    external/Detours/src/disolx86.cpp
    external/Detours/src/image.cpp
    external/Detours/src/modules.cpp
)
target_include_directories(Detours PUBLIC external/Detours/src)
set_target_properties(Detours PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# ---- Add yaml-cpp dependency ----
find_package(yaml-cpp CONFIG REQUIRED)

# ---- Add SQLite3 dependency ----
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# ---- Plugin Target ----
add_library(
    ${PROJECT_NAME}
    SHARED
    src/main.cpp
    src/DialogueHook.cpp
    src/Config.cpp
    src/PopulateTopicInfoHook.cpp
    src/ConstructResponseHook.cpp
    src/SceneHook.cpp
    src/SceneMonitor.cpp
    src/DialogueDatabase.cpp
    src/DialogueLogger.cpp
    src/TopicResponseExtractor.cpp
    src/STFUMenu.cpp
    src/STFUMenu_Settings.cpp
    src/PrismaUIMenu.cpp
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
        CommonLibSSE::CommonLibSSE
        yaml-cpp::yaml-cpp
        unofficial::sqlite3::sqlite3
        Detours
)

# Enable PCH to avoid static assertion issues
# Temporarily disable PCH to debug (same as Physical Diaries)
# target_precompile_headers(
#     ${PROJECT_NAME}
#     PRIVATE
#         include/PCH.h
# )

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
