cmake_minimum_required(VERSION 3.16)
project(fox-page VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Try to find system-installed Lexbor first
find_package(lexbor QUIET)

if(NOT lexbor_FOUND)
    message(STATUS "System lexbor not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        lexbor
        GIT_REPOSITORY https://github.com/lexbor/lexbor.git
        GIT_TAG v2.3.0
    )
    FetchContent_MakeAvailable(lexbor)
    set(LEXBOR_TARGET lexbor)
else()
    message(STATUS "Found system lexbor")
    set(LEXBOR_TARGET lexbor::lexbor)
endif()

# Create fox-page executable
add_executable(fox-page
    src/main.cpp
    src/lexbor_wrapper.cpp
    src/template_processor.cpp
    src/code_generator.cpp
    src/cli.cpp
)

target_include_directories(fox-page PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(fox-page PRIVATE ${LEXBOR_TARGET})

# Compiler flags for warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(fox-page PRIVATE -Wall -Wextra -pedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(fox-page PRIVATE /W4)
endif()

# Install configuration
install(TARGETS fox-page
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# Install documentation (optional)
install(FILES
    spec.md
    README.md
    DESTINATION share/doc/fox-page
    COMPONENT Documentation
    OPTIONAL
)