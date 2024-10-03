cmake_minimum_required(VERSION 3.10)

cmake_policy (SET CMP0054 NEW)
project (Sponge)

set(CMAKE_EXPORT_COMPILECOMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_C_COMPILER clang)
# set(CMAKE_CXX_COMPILER clang++)

include (etc/build_defs.cmake)
include (etc/build_type.cmake)
include (etc/cflags.cmake)

include (etc/doxygen.cmake)

include (etc/clang_format.cmake)
include (etc/clang_tidy.cmake)
include (etc/cppcheck.cmake)

include_directories ("${PROJECT_SOURCE_DIR}/libsponge/util")
include_directories ("${PROJECT_SOURCE_DIR}/libsponge/tcp_helpers")
include_directories ("${PROJECT_SOURCE_DIR}/libsponge")

add_subdirectory ("${PROJECT_SOURCE_DIR}/libsponge")

add_subdirectory ("${PROJECT_SOURCE_DIR}/apps")

add_subdirectory ("${PROJECT_SOURCE_DIR}/tests")

add_subdirectory ("${PROJECT_SOURCE_DIR}/doctests")

include (etc/tests.cmake)
