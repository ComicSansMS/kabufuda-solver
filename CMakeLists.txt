cmake_minimum_required(VERSION 3.20)

project(kabufuda)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/W4>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/permissive->)

find_package(fmt REQUIRED)

add_executable(kabufuda kabufuda.cpp)
target_link_libraries(kabufuda PUBLIC fmt::fmt)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT kabufuda)
