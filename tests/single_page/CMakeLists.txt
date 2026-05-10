# Single-page smoke test: render home.htpl through the in-tree fox-page,
# serve it with fox-http on a random port, and curl/assert the output.

# fox-http is consumed via find_package; install it once with
# `cmake --install` from the fox-http source tree before enabling tests.
find_package(fox-http REQUIRED)

# Generate home.cpp from home.htpl using the in-tree fox-page target.
set(HOME_CPP ${CMAKE_CURRENT_BINARY_DIR}/home.cpp)
add_custom_command(
    OUTPUT ${HOME_CPP}
    COMMAND $<TARGET_FILE:fox-page>
            ${CMAKE_CURRENT_SOURCE_DIR}/home.htpl
            -o ${HOME_CPP}
            --func "void render_home(fox::http::HttpResponse& resp, const std::string& name, const std::string& timestamp, const std::vector<Feature>& features)"
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/home.htpl
            ${CMAKE_CURRENT_SOURCE_DIR}/feature.h
            fox-page
    COMMENT "fox-page: home.htpl → home.cpp"
    VERBATIM)

add_executable(single_page_smoke
    main.cpp
    ${HOME_CPP})

target_include_directories(single_page_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(single_page_smoke PRIVATE
    fox-http::fox-http)

add_test(
    NAME single_page_smoke
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run.sh
            $<TARGET_FILE:single_page_smoke>
            ${CMAKE_CURRENT_SOURCE_DIR}/static)
