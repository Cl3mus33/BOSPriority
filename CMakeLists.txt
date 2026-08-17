cmake_minimum_required (VERSION 3.31)

#
# Project
#
set(PROJECT_NAME "BOSPriority")

# vcpkg's overlay triplet (triplets/x64-windows.cmake) builds every dependency with a static CRT
# (VCPKG_CRT_LINKAGE static), so our own target must match or the link step mismatches (LNK2038)
# against vcpkg's static libs. Must be set before project() - it controls compiler/linker defaults.
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

#
# C++20 for all sources
#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# MSVC / Windows SDK
#
add_compile_options("/Zc:__cplusplus")
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Disables conflicting min/max macros in Windows.h
add_compile_definitions(NOMINMAX)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(_WIN32_WINNT=0x0A00)
add_compile_definitions(UNICODE _UNICODE)

project(${PROJECT_NAME} VERSION 0.1.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#
# Includes and Sources
#
include_directories("include")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

#
# Executable
#
# Plain console subsystem (same as AutoSeasons/CMakeLists.txt) - the GUI still opens fine from a
# console-subsystem exe, and CLI/automation use (spdlog output, --help, etc.) needs the console
# that a WIN32-subsystem exe wouldn't have without extra AllocConsole plumbing.
# packaging/icon/BOSPriority.rc embeds the app icon (taskbar, Explorer, Alt-Tab, title bar) -
# MSVC's resource compiler picks up a .rc source automatically once it's part of the target.
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    packaging/icon/BOSPriority.rc
)
target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
target_include_directories(${PROJECT_NAME} PUBLIC include)

#
# VCPKG Dependencies
#
find_package(CLI11 REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)
find_package(wxWidgets REQUIRED COMPONENTS core base)
find_package(ZLIB REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    ${wxWidgets_LIBRARIES}
    ZLIB::ZLIB
)
target_include_directories(${PROJECT_NAME} PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${wxWidgets_DEFINITIONS})

# No VC Redist DLL copy step: CMAKE_MSVC_RUNTIME_LIBRARY above statically links the CRT into
# the executable, so msvcp140/vcruntime140/concrt140/vccorlib140 aren't runtime dependencies.

# BOSLocale reads its JSONs from a "BOSPriority_translations" folder next to the exe at runtime.
# Copying them on every build keeps a locally built exe behaving like a packaged release - without
# this the build output only ever has whatever was hand-copied there once, so an edited or newly
# added translation key silently appears to do nothing when testing locally.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/translations"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/BOSPriority_translations"
    COMMENT "Copying translation files next to the executable")
