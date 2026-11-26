cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(LuaConsole VERSION 1.0.0)

add_library(${PROJECT_NAME} SHARED src/main.cpp)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Defina a variavel GEODE_SDK")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Lua 5.4 embutido (compilado junto com o mod)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
CPMAddPackage("gh:marovira/lua#5.4.4")
target_link_libraries(${PROJECT_NAME} lua::lib)

setup_geode_mod(${PROJECT_NAME})
