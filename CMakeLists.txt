cmake_minimum_required(VERSION 3.14)
set(NAME ccapi-okx-example)
project(${NAME})

# Set C++17 standard (required by ccapi)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific compiler flags
if(NOT APPLE AND NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# Set build type if not specified
if(WIN32)
  set(CMAKE_BUILD_TYPE "Release")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

# Get ccapi project directory
get_filename_component(CCAPI_PROJECT_DIR ccapi/ ABSOLUTE)
message(STATUS "CCAPI_PROJECT_DIR: ${CCAPI_PROJECT_DIR}")

# Download and setup dependencies automatically
# Check for existing boost directory first
if(EXISTS "${CMAKE_BINARY_DIR}/boost" AND IS_DIRECTORY "${CMAKE_BINARY_DIR}/boost")
  set(BOOST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/boost")
  message(STATUS "Found existing Boost directory: ${BOOST_INCLUDE_DIR}")
  add_library(boost INTERFACE)
elseif(NOT BOOST_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(
    boost
    URL https://archives.boost.io/release/1.87.0/source/boost_1_87_0.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/boost"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND "")
  set(BOOST_INCLUDE_DIR "${CMAKE_BINARY_DIR}/boost")
  message(STATUS "Downloading Boost to: ${BOOST_INCLUDE_DIR}")
else()
  add_library(boost INTERFACE)
endif()

# Check for existing rapidjson directory first
if(EXISTS "${CMAKE_BINARY_DIR}/rapidjson" AND IS_DIRECTORY "${CMAKE_BINARY_DIR}/rapidjson")
  set(RAPIDJSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/rapidjson/include")
  message(STATUS "Found existing RapidJSON directory: ${RAPIDJSON_INCLUDE_DIR}")
  add_library(rapidjson INTERFACE)
elseif(NOT RAPIDJSON_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(
    rapidjson
    URL https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/rapidjson"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND "")
  set(RAPIDJSON_INCLUDE_DIR "${CMAKE_BINARY_DIR}/rapidjson/include")
  message(STATUS "Downloading RapidJSON to: ${RAPIDJSON_INCLUDE_DIR}")
else()
  add_library(rapidjson INTERFACE)
endif()

# Check for existing hffix directory first
if(EXISTS "${CMAKE_BINARY_DIR}/hffix" AND IS_DIRECTORY "${CMAKE_BINARY_DIR}/hffix")
  set(HFFIX_INCLUDE_DIR "${CMAKE_BINARY_DIR}/hffix/include")
  message(STATUS "Found existing HFFIX directory: ${HFFIX_INCLUDE_DIR}")
  add_library(hffix INTERFACE)
elseif(NOT HFFIX_INCLUDE_DIR)
  include(ExternalProject)
  ExternalProject_Add(
    hffix
    URL https://github.com/jamesdbrock/hffix/archive/refs/tags/v1.4.1.tar.gz
    SOURCE_DIR "${CMAKE_BINARY_DIR}/hffix"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    TEST_COMMAND "")
  set(HFFIX_INCLUDE_DIR "${CMAKE_BINARY_DIR}/hffix/include")
  message(STATUS "Downloading HFFIX to: ${HFFIX_INCLUDE_DIR}")
else()
  add_library(hffix INTERFACE)
endif()

# Setup include directories
include_directories(
  ${CCAPI_PROJECT_DIR}/include 
  ${BOOST_INCLUDE_DIR} 
  ${RAPIDJSON_INCLUDE_DIR}
  ${HFFIX_INCLUDE_DIR} 
  ${OPENSSL_INCLUDE_DIR}
)

# Find and link OpenSSL
find_package(OpenSSL REQUIRED)
message(STATUS "OPENSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OPENSSL_CRYPTO_LIBRARY: ${OPENSSL_CRYPTO_LIBRARY}")
message(STATUS "OPENSSL_SSL_LIBRARY: ${OPENSSL_SSL_LIBRARY}")

# Platform-specific link libraries
set(ADDITIONAL_LINK_LIBRARIES "")
if(WIN32)
  set(ADDITIONAL_LINK_LIBRARIES ws2_32)
endif()

# Link libraries
link_libraries(OpenSSL::Crypto OpenSSL::SSL ${ADDITIONAL_LINK_LIBRARIES})

# Find and link ZLIB (required by ccapi for some exchanges)
find_package(ZLIB REQUIRED)
link_libraries(ZLIB::ZLIB)

# Suppress warnings for cleaner output
add_compile_options(-Wno-deprecated -Wno-nonnull -Wno-deprecated-declarations)

# Add optimization flags to prevent potential build issues
if(CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

# Enable required ccapi services and exchanges
add_compile_definitions(CCAPI_ENABLE_SERVICE_MARKET_DATA)
add_compile_definitions(CCAPI_ENABLE_SERVICE_EXECUTION_MANAGEMENT)
add_compile_definitions(CCAPI_ENABLE_EXCHANGE_OKX)

# Create the executable
add_executable(${NAME} src/main.cpp)

# Add dependencies to ensure external projects are built first
add_dependencies(${NAME} boost rapidjson hffix)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Configuration Summary ===")
message(STATUS "Project: ${NAME}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "OpenSSL found: ${OPENSSL_FOUND}")
message(STATUS "ZLIB found: ${ZLIB_FOUND}")
message(STATUS "=============================")
message(STATUS "") 