#include "App.hpp"
#include <print>

int main(int argc, char** argv) {
    static constexpr int window_width{800};
    constexpr int        window_height{600};

    try {
        App app(window_width, window_height, "Tannery");
        app.run();
    } catch (const std::exception& e) {
        std::println(stderr, "Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}
