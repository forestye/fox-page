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

    const std::vector<Feature> features = {
        {"{{expr}} interpolation",
         "values inlined into the iov chain through stringify()"},
        {"cpp-for",
         "the same fragment is rendered per element; loop body produced once at compile time"},
        {"cpp-if",
         "conditional render with no allocation when the branch is empty"},
        {"writev zero-copy",
         "static fragments live in .rodata; dynamic ones live on the handler stack"},
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
    std::cout << "fox-page demo listening on http://127.0.0.1:" << port << "\n";
    return server.run();
}
