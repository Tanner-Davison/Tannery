#include "App.hpp"
#include <print>
#include <taskflow/algorithm/module.hpp>
#include <taskflow/taskflow.hpp>

int main(int argc, char** argv) {
    static constexpr int window_width{800};
    constexpr int        window_height{600};

    tf::Taskflow taskflow;

    int i;

    auto [init, body, cond, done] = taskflow.emplace(
        [&]() {
            std::cout << "i=0\n";
            i = 0;
        },
        [&]() {
            std::cout << "i++ => i=";
            i++;
        },
        [&]() {
            std::cout << i << '\n';
            return i < 5 ? 0 : 1;
        },
        [&]() {
            std::cout << "done\n";
        });

    init.name("init");
    body.name("do i++");
    cond.name("while i<5");
    done.name("done");

    init.precede(body);
    body.precede(cond);
    cond.precede(body, done);

    // taskflow.dump(std::cout);

    std::ofstream os(".cache/taskflow.dot");
    taskflow.dump(os);
    tf::Executor executor;
    executor.run(taskflow).wait();

    try {
        App app(window_width, window_height, "Tannery");
        app.run();
    } catch (const std::exception& e) {
        std::println(stderr, "Fatal error: {}", e.what());
        return 1;
    }
    return 0;
}
