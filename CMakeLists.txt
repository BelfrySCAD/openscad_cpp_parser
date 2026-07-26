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

# ponytail: Windows has no bison/flex at all by default. Prefer MSYS2's
# (kept current -- our grammar needs 3.8+, see parser.y's %require) over the
# winflexbison3 Chocolatey package, whose last release (2021) bundles bison
# 3.7.4 -- confirmed too old by an actual CI failure ("require bison 3.8,
# but have 3.7.4"), not assumed. MSYS2 ships pre-installed on GitHub's
# windows-latest runner image at C:/msys64 (just not on PATH); falls back to
# win_bison/win_flex (e.g. a local dev machine with winflexbison instead) if
# that path doesn't exist.
if(WIN32)
  if(EXISTS "C:/msys64/usr/bin/bison.exe")
    set(BISON_EXECUTABLE "C:/msys64/usr/bin/bison.exe" CACHE FILEPATH "Bison executable" FORCE)
  else()
    find_program(_win_bison_exe NAMES win_bison)
    if(_win_bison_exe)
      set(BISON_EXECUTABLE "${_win_bison_exe}" CACHE FILEPATH "Bison executable" FORCE)
    endif()
  endif()
  if(EXISTS "C:/msys64/usr/bin/flex.exe")
    set(FLEX_EXECUTABLE "C:/msys64/usr/bin/flex.exe" CACHE FILEPATH "Flex executable" FORCE)
  else()
    find_program(_win_flex_exe NAMES win_flex)
    if(_win_flex_exe)
      set(FLEX_EXECUTABLE "${_win_flex_exe}" CACHE FILEPATH "Flex executable" FORCE)
    endif()
  endif()
endif()

find_package(BISON 3.8 REQUIRED)
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
