cmake_minimum_required(VERSION 3.16)

project(mob LANGUAGES CXX)

find_package(clipp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL REQUIRED)

add_subdirectory(src)

set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT mob)
