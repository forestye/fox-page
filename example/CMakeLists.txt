cmake_minimum_required(VERSION 3.16)
project(fox-page-example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── fox-http (sibling project at ../../fox-http) ────────────────────
if(NOT TARGET fox-http::fox-http)
    set(FOX_HTTP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../fox-http
                     ${CMAKE_BINARY_DIR}/fox-http_build)
endif()

# ─── fox-page binary (in-tree, parent's build dir) ───────────────────
find_program(FOX_PAGE NAMES fox-page
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../build)
if(NOT FOX_PAGE)
    message(FATAL_ERROR
        "fox-page not found — build it first: "
        "cmake -S .. -B ../build && cmake --build ../build")
endif()
message(STATUS "fox-page: ${FOX_PAGE}")

# ─── Generate home.cpp from home.htpl ────────────────────────────────
set(HOME_CPP ${CMAKE_BINARY_DIR}/home.cpp)
add_custom_command(
    OUTPUT ${HOME_CPP}
    COMMAND ${FOX_PAGE}
            ${CMAKE_CURRENT_SOURCE_DIR}/home.htpl
            -o ${HOME_CPP}
            --func "void render_home(fox::http::HttpResponse& resp, const std::string& name, const std::string& timestamp, const std::vector<Feature>& features)"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/home.htpl
            ${CMAKE_CURRENT_SOURCE_DIR}/feature.h
            ${FOX_PAGE}
    COMMENT "fox-page: home.htpl → home.cpp"
    VERBATIM)

add_executable(fox-page-demo
    main.cpp
    ${HOME_CPP})

target_include_directories(fox-page-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fox-page-demo PRIVATE fox-http::fox-http)

# Place static/ next to the binary so the demo runs out of the build dir.
add_custom_command(TARGET fox-page-demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/static $<TARGET_FILE_DIR:fox-page-demo>/static
    COMMENT "Copying static/ next to fox-page-demo")
