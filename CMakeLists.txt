cmake_minimum_required(VERSION 3.16)
project(gateway_path CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GEOS CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

add_library(gateway_solver INTERFACE)
target_include_directories(gateway_solver INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(gateway_solver INTERFACE GEOS::geos)

add_executable(server src/main.cpp)
target_link_libraries(server PRIVATE gateway_solver nlohmann_json::nlohmann_json httplib::httplib)
target_compile_options(server PRIVATE -Wall -Wextra)
