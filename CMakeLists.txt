cmake_minimum_required(VERSION 3.30)

# Windows-only project
if(NOT WIN32)
    message(FATAL_ERROR "cpp-bindings-windows can only be built on Windows.")
endif()

# Export compile commands to root directory
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME cmake_git_versioning
    GITHUB_REPOSITORY Katze719/cmake-git-versioning
    GIT_TAG v1.0.1
)

# Include cmake-git-versioning module
include(${cmake_git_versioning_SOURCE_DIR}/cmake/cmake-git-versioning.cmake)

get_git_version_info()

project(
    cpp-bindings-windows
    VERSION "${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH}"
    DESCRIPTION "Windows implementation of cpp-core serial communication bindings"
    LANGUAGES CXX
)

# Generate JSR package metadata from the same git-derived version as the library.
# We generate into the source directory to match the Linux repo behavior.
configure_file(
    "${CMAKE_SOURCE_DIR}/jsr/package/jsr.json.in"
    "${CMAKE_SOURCE_DIR}/jsr/package/jsr.json"
    @ONLY
)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable C++23 module support
set(CMAKE_CXX_MODULE_STD 23)
set(CMAKE_CXX_MODULE_EXTENSIONS OFF)

CPMAddPackage(
    NAME cpp_core
    GITHUB_REPOSITORY Serial-IO/cpp-core
    GIT_TAG main
    OPTIONS
    "CMAKE_EXPORT_COMPILE_COMMANDS OFF"
)

# Generate version information
generate_git_version(
    OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated
    OUTPUT_FILE version.hpp
)

CPMAddPackage(
    NAME GTest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.14.0
    OPTIONS
    "INSTALL_GTEST OFF"
    "gtest_force_shared_crt ON"
)

include(CTest)
enable_testing()

file(
    GLOB_RECURSE LIB_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(cpp_bindings_windows SHARED ${LIB_SOURCES})

set_target_properties(
    cpp_bindings_windows
    PROPERTIES
    VERSION "${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH}"
    SOVERSION "${GIT_VERSION_MAJOR}"
    OUTPUT_NAME cpp_bindings_windows
)

target_include_directories(
    cpp_bindings_windows
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
)

target_link_libraries(
    cpp_bindings_windows
    PUBLIC
    cpp_core::cpp_core
)

# cpp-core's `MODULE_API` macro checks for `cpp_windows_bindings_EXPORTS` on Windows.
# Our target is named `cpp_bindings_windows`, so CMake would otherwise define
# `cpp_bindings_windows_EXPORTS` and `MODULE_API` would resolve to dllimport.
target_compile_definitions(cpp_bindings_windows PRIVATE cpp_windows_bindings_EXPORTS)

target_compile_features(cpp_bindings_windows PUBLIC cxx_std_23)

# Collect all test source files
file(
    GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

if(TEST_SOURCES)
    add_executable(cpp_bindings_windows_tests ${TEST_SOURCES})

    target_include_directories(
        cpp_bindings_windows_tests
        PRIVATE
        ${CMAKE_BINARY_DIR}/generated
    )

    target_link_libraries(
        cpp_bindings_windows_tests
        PRIVATE
        cpp_bindings_windows
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )

    target_compile_features(cpp_bindings_windows_tests PRIVATE cxx_std_23)

    include(GoogleTest)
    gtest_discover_tests(cpp_bindings_windows_tests)
endif()

include(GNUInstallDirs)

install(
    TARGETS cpp_bindings_windows
    EXPORT cpp_bindings_windowsTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Copy generated version header
install(
    FILES ${CMAKE_BINARY_DIR}/generated/version.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    OPTIONAL
)

if(CMAKE_EXPORT_COMPILE_COMMANDS AND EXISTS "${CMAKE_BINARY_DIR}/compile_commands.json")
    add_custom_target(
        copy-compile-commands
        ALL
        ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_SOURCE_DIR}/compile_commands.json
        DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
        COMMENT "Copying compile_commands.json to project root"
    )
endif()


