cmake_minimum_required(VERSION 3.16)
project(Thermoviewer98 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---- FLTK as a sub-project, statically linked, only what we need ----
set(FLTK_BUILD_SHARED_LIBS     OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_TEST            OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_EXAMPLES        OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FLUID           OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FLTK_OPTIONS    OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_FORMS           OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_GL              OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_HTML_DOCS       OFF CACHE BOOL "" FORCE)
set(FLTK_BUILD_PDF_DOCS        OFF CACHE BOOL "" FORCE)
set(FLTK_OPTION_SVG            OFF CACHE BOOL "" FORCE)
set(FLTK_OPTION_PRINT_SUPPORT  OFF CACHE BOOL "" FORCE)
set(FLTK_USE_SYSTEM_LIBPNG     OFF CACHE BOOL "" FORCE)
set(FLTK_USE_SYSTEM_ZLIB       OFF CACHE BOOL "" FORCE)
set(FLTK_USE_SYSTEM_LIBJPEG    OFF CACHE BOOL "" FORCE)
set(FLTK_BACKEND_WAYLAND       OFF CACHE BOOL "" FORCE)  # X11 (runs on Wayland via XWayland)
set(FLTK_USE_PANGO             OFF CACHE BOOL "" FORCE)
set(FLTK_USE_DBUS              OFF CACHE BOOL "" FORCE)
set(FLTK_GRAPHICS_GDIPLUS      OFF CACHE BOOL "" FORCE)  # pixel-exact GDI lines (Win98 look)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fltk/CMakeLists.txt)
  add_subdirectory(third_party/fltk EXCLUDE_FROM_ALL)
else()
  # FLTK is fetched from GitHub on first configure (needs internet once)
  include(FetchContent)
  FetchContent_Declare(fltk
    GIT_REPOSITORY https://github.com/fltk/fltk.git
    GIT_TAG        release-1.4.4
    GIT_SHALLOW    TRUE)
  FetchContent_MakeAvailable(fltk)
endif()

set(SOURCES
  src/main.cpp
  src/irb.cpp
  src/win98.cpp
  src/thermoview.cpp
)
if(WIN32)
  list(APPEND SOURCES res/app.rc)
  add_executable(thermoviewer98 WIN32 ${SOURCES})
else()
  add_executable(thermoviewer98 ${SOURCES})
endif()

target_include_directories(thermoviewer98 PRIVATE src)
target_link_libraries(thermoviewer98 PRIVATE fltk::images fltk::fltk)

if(MINGW)
  target_link_options(thermoviewer98 PRIVATE -static -static-libgcc -static-libstdc++ -s)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_options(thermoviewer98 PRIVATE -static-libstdc++ -static-libgcc -s)
endif()
if(MSVC)
  target_compile_options(thermoviewer98 PRIVATE /W3 /utf-8)
else()
  target_compile_options(thermoviewer98 PRIVATE -Wall -O2)
endif()

# Small command line tool: IRB -> CSV (no GUI), same core
add_executable(irb2csv src/irb2csv.cpp src/irb.cpp)
target_include_directories(irb2csv PRIVATE src)
if(MINGW)
  target_link_options(irb2csv PRIVATE -static -s)
endif()
