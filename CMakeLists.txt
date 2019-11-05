cmake_minimum_required(VERSION 3.10)

project(ws C CXX)

add_library(ws INTERFACE)
target_include_directories(ws INTERFACE ${PROJECT_SOURCE_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if("${CMAKE_CXX_COMPILER}" MATCHES "clang")
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
find_package(Threads)
list(APPEND LIBS Threads::Threads)
list(APPEND INC ${PROJECT_SOURCE_DIR})

if(NOT ASIO_PATH)
  set(ASIO_PATH /opt/asio-1.12.2/include)
endif()

if(NOT EXISTS ${ASIO_PATH})
  message(FATAL_ERROR "you must supply asio directory which contains asio.hpp by using -DASIO_PATH=/path/to/asio")
else()
  list(APPEND INC "${ASIO_PATH}")
  add_definitions(-DASIO_STANDALONE -DASIO_HAS_STD_CHRONO)
endif()

include_directories(${INC})
add_executable(echo_server examples/common.h examples/echo_server.cpp)
target_link_libraries(echo_server ${LIBS})

add_executable(push_server examples/common.h examples/push_server.cpp)
target_link_libraries(push_server ${LIBS})

add_executable(echo_client examples/common.h examples/echo_client.cpp)
target_link_libraries(echo_client ${LIBS})

add_executable(push_client examples/common.h examples/push_client.cpp)
target_link_libraries(push_client ${LIBS})

option(WITH_SSL "build ssl examples" ON)

if(WITH_SSL)
  find_package(OpenSSL REQUIRED)
  include_directories(${OPENSSL_INCLUDE_DIR})
  add_executable(ssl_echo_client examples/common.h examples/ssl_echo_client.cpp)
  target_link_libraries(ssl_echo_client ${LIBS} ${OPENSSL_LIBRARIES})

  add_executable(ssl_echo_server examples/common.h examples/ssl_echo_server.cpp)
  target_link_libraries(ssl_echo_server ${LIBS} ${OPENSSL_LIBRARIES})

  add_custom_command(
          TARGET ssl_echo_client POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/examples/key.pem
          ${CMAKE_SOURCE_DIR}/examples/cert.pem
          ${EXECUTABLE_OUTPUT_PATH}/)
endif()