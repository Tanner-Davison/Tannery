#include "Window.hpp"
#include <print>
#include <stdexcept>

inline static void error_callback(int error_code, const char* description) {
    std::print(stderr, "GLFW error {}: {}\n", error_code, description);
};

inline static void key_callback(GLFWwindow* window,
                                int         key,
                                int         scancode,
                                int         action,
                                int         mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

Window::Window(int width, int height, const char* title) {
    glfwSetErrorCallback(error_callback);

#ifdef __linux__
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif

    if (!glfwInit()) {
        throw std::runtime_error("Error: GLFW Init() failed. Failed to create Window");
    }
    // Window Hints
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    this->window = glfwCreateWindow(width, height, title, NULL, NULL);

    if (!window) {
        glfwTerminate();
        throw std::runtime_error("GLFW Window Creation Failed");
    }

    glfwSetKeyCallback(window, key_callback);
}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

GLFWwindow* Window::handle() const {
    return this->window;
};
