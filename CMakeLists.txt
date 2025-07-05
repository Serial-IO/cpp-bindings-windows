cmake_minimum_required(VERSION 3.30)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 2)
set(VERSION_PATCH 0)

set(PROJECT_N cpp_unix_bindings)
project(${PROJECT_N} VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

# Generate compile_commands.json for clang-based tooling (clangd / clang-tidy)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME "googletest"
    GITHUB_REPOSITORY "google/googletest"
    GIT_TAG "v1.17.0"
    GIT_SHALLOW TRUE
    OPTIONS "GTEST_BUILD_TESTS OFF"
    OPTIONS "GTEST_BUILD_EXAMPLES OFF"
    OPTIONS "GTEST_BUILD_DOCS OFF"
)

# Ensure clang tooling can pick up compile_commands.json from project root
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    add_custom_target(copy-compile-commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${PROJECT_SOURCE_DIR}/compile_commands.json
        COMMENT "Copy compile_commands.json to project root" 
        VERBATIM)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/src/**.cpp)

set(LIB true)

configure_file(versioning/version_config.cpp.in ${PROJECT_SOURCE_DIR}/src/version_config.cpp)

# a macro that gets all of the header containing directories. 
MACRO(header_directories return_list includes_base_folder extention )
    FILE(GLOB_RECURSE new_list ${includes_base_folder}/*.${extention})
    SET(dir_list "")
    FOREACH(file_path ${new_list})
        GET_FILENAME_COMPONENT(dir_path ${file_path} PATH)
        SET(dir_list ${dir_list} ${dir_path})
    ENDFOREACH()
    LIST(REMOVE_DUPLICATES dir_list)
    SET(${return_list} ${dir_list})
ENDMACRO()

# using said macro.
header_directories(INCLUDES ${PROJECT_SOURCE_DIR}/src/ hpp)

message("src files:")
foreach(file ${SRCS})
        message(STATUS ${file})
endforeach()

message("include directories:")
foreach(dir ${INCLUDES})
        message(STATUS ${dir})
endforeach()

if(LIB)
    add_library(${PROJECT_N} SHARED ${SRCS})
else()
    add_executable(${PROJECT_N} ${SRCS}) 
endif(LIB)

set_target_properties(${PROJECT_N} PROPERTIES 
                              VERSION ${PROJECT_VERSION}
                            SOVERSION ${PROJECT_VERSION_MAJOR})

target_include_directories(${PROJECT_N} PUBLIC ${PROJECT_SOURCE_DIR}/src)

add_executable(tests
    tests/serial_test.cpp
    tests/serial_unit_tests.cpp)

target_link_libraries(tests PRIVATE ${PROJECT_N} gtest)

target_include_directories(tests PRIVATE ${PROJECT_SOURCE_DIR}/src)

add_custom_command(TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${PROJECT_N}>
            $<TARGET_FILE_DIR:tests>
    COMMENT "Copy shared library next to aggregated test binary")

enable_testing()
add_test(NAME AllTests COMMAND tests /dev/ttyUSB0)
