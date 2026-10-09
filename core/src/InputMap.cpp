#include "InputMap.hpp"

InputMap::InputMap() {
    bind(Action::MoveForward, GLFW_KEY_W);
    bind(Action::MoveBackward, GLFW_KEY_S);
    bind(Action::MoveLeft, GLFW_KEY_A);
    bind(Action::MoveRight, GLFW_KEY_D);
    bind(Action::MoveUp, GLFW_KEY_E);
    bind(Action::MoveUp, GLFW_KEY_SPACE);
    bind(Action::MoveDown, GLFW_KEY_Q);
    bind(Action::MoveDown, GLFW_KEY_LEFT_SHIFT);
    bind(Action::SensitivityDown, GLFW_KEY_LEFT_BRACKET);
    bind(Action::SensitivityUp, GLFW_KEY_RIGHT_BRACKET);
    bind(Action::Quit, GLFW_KEY_ESCAPE);
}

void InputMap::bind(Action pAction, int pKey) {
    keys[index(pAction)].push_back(pKey);
}

void InputMap::rebind(Action pAction, int pKey) {
    keys[index(pAction)].assign(1, pKey);
}

bool InputMap::isDown(GLFWwindow* pWindow, Action pAction) const {
    for (const int key : keys[index(pAction)]) {
        if (glfwGetKey(pWindow, key) == GLFW_PRESS) {
            return true;
        }
    }
    return false;
}
