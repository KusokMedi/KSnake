cmake_minimum_required(VERSION 3.10)
project(KSnake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_path(SDL2_INCLUDE SDL.h PATH_SUFFIXES SDL2)
    find_library(SDL2_LIB SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
endif()

find_path(SDL2TTF_INCLUDE SDL_ttf.h PATH_SUFFIXES SDL2)
find_library(SDL2TTF_LIB SDL2_ttf)
if(NOT SDL2TTF_INCLUDE OR NOT SDL2TTF_LIB)
    message(FATAL_ERROR "SDL2_ttf not found. Install sdl2_ttf package.")
endif()

add_executable(snake src/main.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(snake PROPERTIES WIN32_EXECUTABLE TRUE)
    target_include_directories(snake PRIVATE
        ${SDL2_INCLUDE} ${SDL2TTF_INCLUDE} ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(snake ${SDL2_LIB} ${SDL2TTF_LIB} -static)
else()
    target_include_directories(snake PRIVATE
        ${SDL2TTF_INCLUDE} ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(snake PkgConfig::SDL2 ${SDL2TTF_LIB})
endif()