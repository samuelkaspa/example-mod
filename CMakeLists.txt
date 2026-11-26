cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(LuaConsole VERSION 1.0.0 LANGUAGES C CXX)

add_library(${PROJECT_NAME} SHARED src/main.cpp)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Defina a variavel GEODE_SDK")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Lua 5.4 - baixa o codigo oficial e compila junto
CPMAddPackage(
    NAME lua_src
    URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
    DOWNLOAD_ONLY YES
)

file(GLOB LUA_SOURCES ${lua_src_SOURCE_DIR}/src/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${lua_src_SOURCE_DIR}/src/lua.c
    ${lua_src_SOURCE_DIR}/src/luac.c
)

add_library(lua_lib STATIC ${LUA_SOURCES})
target_include_directories(lua_lib PUBLIC ${lua_src_SOURCE_DIR}/src)
set_target_properties(lua_lib PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(${PROJECT_NAME} lua_lib)

setup_geode_mod(${PROJECT_NAME})
