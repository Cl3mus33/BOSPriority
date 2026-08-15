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
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
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

target_link_libraries(${PROJECT_NAME} PRIVATE
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    ${wxWidgets_LIBRARIES}
)
target_include_directories(${PROJECT_NAME} PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${wxWidgets_DEFINITIONS})

# No VC Redist DLL copy step: CMAKE_MSVC_RUNTIME_LIBRARY above statically links the CRT into
# the executable, so msvcp140/vcruntime140/concrt140/vccorlib140 aren't runtime dependencies.
