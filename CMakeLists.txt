cmake_minimum_required(VERSION 3.16)
project(bowtie_corvus_json_schema CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The C library's release package (its CMake package), linked statically.
find_package(corvus_json_schema REQUIRED)
find_package(nlohmann_json REQUIRED)
add_executable(bowtie_corvus_json_schema bowtie_corvus_json_schema.cpp)
target_link_libraries(bowtie_corvus_json_schema PRIVATE corvus_json_schema::corvus_json_schema_static nlohmann_json::nlohmann_json)
