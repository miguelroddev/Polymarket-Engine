cmake_minimum_required(VERSION 3.16)
project(polymarket_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#defaults
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_library(polymarket_core
  src/parse.cpp
  src/wsconnect.cpp
  src/app.cpp
)

add_executable(polymarket_engine
  src/main.cpp
)

enable_testing()
add_executable(test_parse tests/test_parse.cpp)
target_link_libraries(test_parse PRIVATE polymarket_core)
add_test(NAME test_parse COMMAND test_parse)

target_link_libraries(polymarket_engine PRIVATE polymarket_core)

target_include_directories(polymarket_engine PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Warnings
if(MSVC)
  target_compile_options(polymarket_engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(polymarket_engine PRIVATE 
  -Wall 
  -Wextra 
  -Wpedantic)
endif()

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

# Link libraries
target_link_libraries(polymarket_engine PRIVATE
    Boost::json
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)
