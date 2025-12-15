cmake_minimum_required(VERSION 3.21)
project(terra_clone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TERRARIA_BUILD_WARNINGS "Enable strict warnings" ON)

file(GLOB_RECURSE TERRARIA_HEADERS CONFIGURE_DEPENDS "include/*.h")
file(GLOB_RECURSE TERRARIA_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(terra_clone ${TERRARIA_HEADERS} ${TERRARIA_SOURCES})
target_include_directories(terra_clone PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(SDL2 REQUIRED)
target_link_libraries(terra_clone PRIVATE SDL2::SDL2)
if (TARGET SDL2::SDL2main)
    target_link_libraries(terra_clone PRIVATE SDL2::SDL2main)
endif()

if (TERRARIA_BUILD_WARNINGS)
    if (MSVC)
        target_compile_options(terra_clone PRIVATE /W4 /permissive- /Zc:__cplusplus)
    else()
        target_compile_options(terra_clone PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    endif()
endif()
