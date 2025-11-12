cmake_minimum_required(VERSION 3.16)

# Set your project name
set(CMAKE_CXX_STANDARD 23)
set(PROJECT_NAME BCForth)

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS ON)





include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${PROJECT_NAME})
idf_build_set_property(CXX_COMPILE_OPTIONS "-fcoroutines" APPEND)