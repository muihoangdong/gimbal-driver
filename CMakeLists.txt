cmake_minimum_required(VERSION 3.20)
project(gimbal_driver VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(GIMBAL_BUILD_TESTS "Build unit tests (tai them GoogleTest)" OFF)

find_package(Threads REQUIRED)

# Chi tai dung 2 file header can dung (~1,2 MB), kiem tra SHA-256 de khoa phien ban.
set(GIMBAL_DEPS_DIR ${CMAKE_BINARY_DIR}/deps/include)
function(gimbal_download url dest sha256)
  if(NOT EXISTS ${dest})
    file(DOWNLOAD ${url} ${dest} EXPECTED_HASH SHA256=${sha256} TLS_VERIFY ON STATUS status)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
      file(REMOVE ${dest})
      message(FATAL_ERROR "Tai ${url} that bai: ${status}")
    endif()
  endif()
endfunction()
gimbal_download(https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.3/httplib.h
  ${GIMBAL_DEPS_DIR}/httplib.h
  a0a0c13dc086663863dbe6730a19716f7d3744904b6205496e8301750814dbd5)
gimbal_download(https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
  ${GIMBAL_DEPS_DIR}/nlohmann/json.hpp
  9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6)

add_library(gimbal_deps INTERFACE)
target_include_directories(gimbal_deps SYSTEM INTERFACE ${GIMBAL_DEPS_DIR})
target_link_libraries(gimbal_deps INTERFACE Threads::Threads)
if(WIN32)
  target_link_libraries(gimbal_deps INTERFACE ws2_32 crypt32)
endif()

add_library(gimbal_driver
  src/gimbal_driver.cpp
  src/mock_driver.cpp
  src/bridge_http_driver.cpp
  src/link_monitor.cpp
  src/service.cpp
  src/config.cpp
  src/log.cpp)
target_include_directories(gimbal_driver PUBLIC include)
target_link_libraries(gimbal_driver
  PUBLIC Threads::Threads
  PRIVATE gimbal_deps)
if(WIN32)
  target_compile_definitions(gimbal_driver PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)
endif()
if(MSVC)
  target_compile_options(gimbal_driver PRIVATE /W4 /utf-8)
else()
  target_compile_options(gimbal_driver PRIVATE -Wall -Wextra)
endif()

add_executable(gimbal-driver src/main.cpp)
target_link_libraries(gimbal-driver PRIVATE gimbal_driver)

add_executable(basic_move examples/basic_move.cpp)
target_link_libraries(basic_move PRIVATE gimbal_driver)

foreach(tool bench_g01 soak_test link_loss_test)
  add_executable(${tool} tools/${tool}.cpp)
  target_link_libraries(${tool} PRIVATE gimbal_driver)
endforeach()

if(GIMBAL_BUILD_TESTS)
  include(FetchContent)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  # DOWNLOAD_EXTRACT_TIMESTAMP chi co tu CMake 3.24; Ubuntu 22.04 tren Pi dung 3.22.
  if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
  endif()
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz)
  FetchContent_MakeAvailable(googletest)

  enable_testing()
  add_executable(gimbal_tests tests/test_driver.cpp tests/test_service.cpp)
  target_link_libraries(gimbal_tests PRIVATE
    gimbal_driver gimbal_deps GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(gimbal_tests DISCOVERY_TIMEOUT 30)
endif()

install(TARGETS gimbal-driver bench_g01 soak_test link_loss_test RUNTIME DESTINATION bin)
