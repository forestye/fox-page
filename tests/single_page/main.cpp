#include "feature.h"

#include "fox-http/http_handler.h"
#include "fox-http/http_request.h"
#include "fox-http/http_response.h"
#include "fox-http/http_router.h"
#include "fox-http/http_server.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

void render_home(fox::http::HttpResponse& resp,
                 const std::string& name,
                 const std::string& timestamp,
                 const std::vector<Feature>& features);

namespace {

std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string current_timestamp() {
    std::time_t t = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

} // namespace

int main(int argc, char* argv[]) {
    using namespace fox::http;

    unsigned short port = (argc > 1) ? static_cast<unsigned short>(std::atoi(argv[1])) : 8080;
    std::string css_path = (argc > 2) ? argv[2] : "static/pico.min.css";

    std::string pico_css;
    try {
        pico_css = read_file(css_path);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load Pico CSS: " << e.what() << "\n"
                  << "Hint: run from a directory with static/pico.min.css next to it,\n"
                  << "      or pass an explicit path: ./fox-page-demo <port> <css_path>\n";
        return 1;
    }

    // Ten entries with intentionally short titles (≤15 bytes → SSO).
    // The cpp-for body pushes two strings per iteration, so 10 features
    // means 20 dynamic strings + name + timestamp = 22 pushes. Any
    // codegen that grows the dynamic-strings container with reallocation
    // (e.g. an old `std::vector` with reserve(16)) will move the SSO
    // titles, leaving stale iov pointers and corrupted output. The
    // notes are deliberately long (>15 bytes) so they live on the
    // heap and are unaffected by container reallocation — that
    // contrast is the diagnostic.
    const std::vector<Feature> features = {
        {"text",   "cpp-text: replace inner text with an expression"},
        {"if",     "cpp-if: conditional render"},
        {"for",    "cpp-for: loop render — the directive this test exercises"},
        {"attr",   "cpp:<name>: dynamic attribute value"},
        {"head",   "x-cpp-head: file-top includes / typedefs"},
        {"source", "x-cpp-source: locals at the top of the generated function"},
        {"tail",   "x-cpp-tail: cleanup right before the function returns"},
        {"interp", "{{expr}}: expression interpolation in text or attributes"},
        {"writev", "iovec scatter-gather: single syscall, zero-copy send"},
        {"sso",    "regression coverage for small-string optimization (titles ≤15B)"},
    };

    HttpRouter router;

    router.get("/", [&](HttpRequest&, HttpResponse& resp) {
        render_home(resp, "world", current_timestamp(), features);
    });

    router.get("/hello/:name", [&](HttpRequest& req, HttpResponse& resp) {
        render_home(resp, std::string(req.param("name")),
                    current_timestamp(), features);
    });

    router.get("/static/pico.min.css", [&](HttpRequest&, HttpResponse& resp) {
        resp.headers().content_type("text/css; charset=utf-8");
        resp.set_body(pico_css);
    });

    HttpServer server(port);
    server.set_handler(&router);
    std::cout << "fox-page demo listening on http://0.0.0.0:" << port << "\n";
    return server.run();
}
