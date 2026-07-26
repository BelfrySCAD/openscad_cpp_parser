cmake_minimum_required(VERSION 3.24)
project(openscad_cpp_parser CXX)

cmake_policy(SET CMP0135 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ponytail: macOS ships an ancient (GPLv2, 2.3) bison; point CMake at a
# homebrew bison (3.5+) if one is present, since our grammar needs
# %skeleton "lalr1.cc" + api.value.type variant support.
if(APPLE AND EXISTS "/opt/homebrew/opt/bison/bin/bison")
  set(BISON_EXECUTABLE "/opt/homebrew/opt/bison/bin/bison" CACHE FILEPATH "Bison executable" FORCE)
elseif(APPLE AND EXISTS "/usr/local/opt/bison/bin/bison")
  set(BISON_EXECUTABLE "/usr/local/opt/bison/bin/bison" CACHE FILEPATH "Bison executable" FORCE)
endif()

# ponytail: Windows has no bison/flex at all by default; the winflexbison
# Chocolatey package (`choco install winflexbison3`) provides win_bison.exe/
# win_flex.exe on PATH, not binaries literally named bison/flex -- point
# CMake at those explicitly (mirrors the macOS Homebrew override above)
# rather than relying on FindBISON/FindFLEX's own search to happen to know
# the Windows-specific name.
if(WIN32)
  find_program(_win_bison_exe NAMES win_bison)
  if(_win_bison_exe)
    set(BISON_EXECUTABLE "${_win_bison_exe}" CACHE FILEPATH "Bison executable" FORCE)
  endif()
  find_program(_win_flex_exe NAMES win_flex)
  if(_win_flex_exe)
    set(FLEX_EXECUTABLE "${_win_flex_exe}" CACHE FILEPATH "Flex executable" FORCE)
  endif()
endif()

find_package(BISON 3.5 REQUIRED)
find_package(FLEX REQUIRED)

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

option(BUILD_TESTING "Build tests" ON)

add_subdirectory(src)
add_subdirectory(tools/cli)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
