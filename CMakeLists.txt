cmake_minimum_required(VERSION 3.16)
project(polymarket_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#defaults
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(polymarket_engine
  src/main.cpp
  # to be added more .cpp files
)

target_include_directories(polymarket_engine PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Warnings (optional but recommended)
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
