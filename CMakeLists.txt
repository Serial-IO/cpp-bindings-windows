cmake_minimum_required(VERSION 3.30)

# Export compile commands to root directory
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME cmake_git_versioning
    GITHUB_REPOSITORY Katze719/cmake-git-versioning
    GIT_TAG v1.1.0
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

# Check after project() so cross-compilation toolchains can initialize WIN32.
if(NOT WIN32)
    message(FATAL_ERROR "cpp-bindings-windows can only be built for Windows.")
endif()

file(WRITE "${CMAKE_BINARY_DIR}/env.bat" "set PACKAGE_VERSION=${GIT_DESCRIBE_NO_V}\n")

# Set C++ standard
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable C++26 module support
set(CMAKE_CXX_MODULE_STD 26)
set(CMAKE_CXX_MODULE_EXTENSIONS OFF)

option(
    CPP_BINDINGS_WINDOWS_ENABLE_FFI_JSON_EXPORT
    "Enable ASTrein JSON export for the cpp-core FFI headers"
    OFF
)
option(
    CPP_BINDINGS_WINDOWS_STATIC_MSVC_RUNTIME
    "Statically link the MSVC runtime into the shared library"
    OFF
)
set(
    CPP_BINDINGS_WINDOWS_ASTREIN_EXECUTABLE
    ""
    CACHE FILEPATH
    "Path to the ASTrein executable used for FFI JSON export"
)
set(
    CPP_BINDINGS_WINDOWS_FFI_JSON_OUTPUT
    "${CMAKE_BINARY_DIR}/cpp_bindings_windows.ffi.json"
    CACHE FILEPATH
    "Output path for the generated cpp-core FFI API metadata"
)

CPMAddPackage(
    NAME cpp_core
    GITHUB_REPOSITORY Serial-IO/cpp-core
    GIT_TAG v2.0.1
    OPTIONS
    "CMAKE_EXPORT_COMPILE_COMMANDS OFF"
)

if(CPP_BINDINGS_WINDOWS_ENABLE_FFI_JSON_EXPORT)
    if(CPP_BINDINGS_WINDOWS_ASTREIN_EXECUTABLE)
        set(_cpp_bindings_windows_astrein "${CPP_BINDINGS_WINDOWS_ASTREIN_EXECUTABLE}")
    else()
        find_program(_cpp_bindings_windows_astrein NAMES astrein astrein.exe)
    endif()

    if(NOT _cpp_bindings_windows_astrein)
        message(
            FATAL_ERROR
            "CPP_BINDINGS_WINDOWS_ENABLE_FFI_JSON_EXPORT=ON requires ASTrein. "
            "Install astrein or set CPP_BINDINGS_WINDOWS_ASTREIN_EXECUTABLE."
        )
    endif()

    file(
        GLOB_RECURSE _cpp_bindings_windows_ffi_headers
        CONFIGURE_DEPENDS
        "${cpp_core_SOURCE_DIR}/include/*.h"
        "${cpp_core_SOURCE_DIR}/include/*.hpp"
    )
    set(_cpp_bindings_windows_ffi_wrapper "${CMAKE_BINARY_DIR}/ffi.cpp")
    get_filename_component(
        _cpp_bindings_windows_ffi_output_dir
        "${CPP_BINDINGS_WINDOWS_FFI_JSON_OUTPUT}"
        DIRECTORY
    )

    file(
        GENERATE
        OUTPUT "${_cpp_bindings_windows_ffi_wrapper}"
        CONTENT "#include <cpp_core/serial.h>\n"
    )

    add_library(
        cpp_bindings_windows_ffi_ast_context
        OBJECT
        EXCLUDE_FROM_ALL
        "${_cpp_bindings_windows_ffi_wrapper}"
    )
    target_include_directories(
        cpp_bindings_windows_ffi_ast_context
        PRIVATE
        "${cpp_core_SOURCE_DIR}/include"
    )
    target_compile_definitions(
        cpp_bindings_windows_ffi_ast_context
        PRIVATE
        cpp_bindings_windows_EXPORTS
    )
    target_compile_features(cpp_bindings_windows_ffi_ast_context PRIVATE cxx_std_26)

    add_custom_command(
        OUTPUT "${CPP_BINDINGS_WINDOWS_FFI_JSON_OUTPUT}"
        COMMAND
            ${CMAKE_COMMAND} -E make_directory
            "${_cpp_bindings_windows_ffi_output_dir}"
        COMMAND
            "${_cpp_bindings_windows_astrein}"
            --ffi
            --compile-commands "${CMAKE_BINARY_DIR}/compile_commands.json"
            --require-c-linkage
            --require-default-visibility
            --public-header "cpp_core/serial.h"
            --api-root "${cpp_core_SOURCE_DIR}/include"
            --output "${CPP_BINDINGS_WINDOWS_FFI_JSON_OUTPUT}"
            "${_cpp_bindings_windows_ffi_wrapper}"
        DEPENDS
            "${_cpp_bindings_windows_astrein}"
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${_cpp_bindings_windows_ffi_wrapper}"
            ${_cpp_bindings_windows_ffi_headers}
        COMMENT "Exporting cpp-core FFI API metadata with ASTrein"
        VERBATIM
    )

    add_custom_target(
        cpp_bindings_windows_ffi_json
        DEPENDS "${CPP_BINDINGS_WINDOWS_FFI_JSON_OUTPUT}"
    )
endif()

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
    "BUILD_GMOCK OFF"
)

include(CTest)
enable_testing()

# Library sources: src/*.cpp only, exclude *.test.cpp and test_helpers/
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*\\.test\\.cpp$")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*/test_helpers/.*")

add_library(cpp_bindings_windows SHARED ${LIB_SOURCES})

set_target_properties(
    cpp_bindings_windows
    PROPERTIES
    VERSION "${GIT_VERSION_MAJOR}.${GIT_VERSION_MINOR}.${GIT_VERSION_PATCH}"
    SOVERSION "${GIT_VERSION_MAJOR}"
    OUTPUT_NAME cpp_bindings_windows
    PREFIX ""
)

target_include_directories(
    cpp_bindings_windows
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
    $<BUILD_INTERFACE:${cpp_core_SOURCE_DIR}/include>
)

target_link_libraries(
    cpp_bindings_windows
    PRIVATE
    cpp_core::cpp_core
    setupapi
)

target_compile_features(cpp_bindings_windows PRIVATE cxx_std_26)

if(MSVC AND CPP_BINDINGS_WINDOWS_STATIC_MSVC_RUNTIME)
    set_property(
        TARGET cpp_bindings_windows
        PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

# Test sources: src/*.test.cpp, tests/*.test.cpp, src/test_helpers/*.cpp (helpers excluded from lib)
file(GLOB SRC_UNIT_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.test.cpp")
file(GLOB TESTS_INTEGRATION CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.test.cpp")
file(GLOB TEST_HELPER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/test_helpers/*.cpp")
set(TEST_SOURCES ${SRC_UNIT_TESTS} ${TESTS_INTEGRATION} ${TEST_HELPER_SOURCES})

if(TEST_SOURCES)
    add_executable(cpp_bindings_windows_tests ${TEST_SOURCES})

    target_include_directories(
        cpp_bindings_windows_tests
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated
        ${cpp_core_SOURCE_DIR}/include
    )

    target_link_libraries(
        cpp_bindings_windows_tests
        PRIVATE
        cpp_bindings_windows
        GTest::gtest
        GTest::gtest_main
    )

    target_compile_features(cpp_bindings_windows_tests PRIVATE cxx_std_26)

    include(GoogleTest)
    if(CMAKE_CROSSCOMPILING)
        gtest_add_tests(TARGET cpp_bindings_windows_tests)
    else()
        gtest_discover_tests(cpp_bindings_windows_tests)
    endif()
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
