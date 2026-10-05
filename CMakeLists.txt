cmake_minimum_required(VERSION 3.10)
project(KSnake LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# Без явного типа сборка шла с пустыми флагами, то есть -O0: и медленно, и
# предупреждения привыкли не показывать. Release по умолчанию, остальное
# переопределяется через -DCMAKE_BUILD_TYPE=Debug.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Предупреждения включены постоянно, а не только в CI: без них -Wall -Wextra
# из AGENTS.md приходилось бы передавать руками при каждой проверке.
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_path(SDL2_INCLUDE SDL.h PATH_SUFFIXES SDL2)
    find_library(SDL2_LIB SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

find_path(SDL2TTF_INCLUDE SDL_ttf.h PATH_SUFFIXES SDL2)
find_library(SDL2TTF_LIB SDL2_ttf)
if(NOT SDL2TTF_INCLUDE OR NOT SDL2TTF_LIB)
    message(FATAL_ERROR "SDL2_ttf not found. Install sdl2_ttf package.")
endif()

add_executable(snake
    src/main.cpp
    src/game.cpp
    src/screens.cpp
    src/storage.cpp
    src/ui.cpp
    src/update.cpp
    src/util.cpp
    src/version.cpp
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_target_properties(snake PROPERTIES WIN32_EXECUTABLE TRUE)
    target_include_directories(snake PRIVATE
        ${SDL2_INCLUDE} ${SDL2TTF_INCLUDE} ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(snake ${SDL2_LIB} ${SDL2TTF_LIB} Threads::Threads shell32 -static)
else()
    target_include_directories(snake PRIVATE
        ${SDL2TTF_INCLUDE} ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(snake PkgConfig::SDL2 ${SDL2TTF_LIB} Threads::Threads)
endif()