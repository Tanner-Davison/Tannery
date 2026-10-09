#pragma once

#include <GLFW/glfw3.h>

#include <array>
#include <cstddef>
#include <vector>

// What the player *means*, independent of which key says it. Game code asks about actions;
// only InputMap knows about physical keys.
enum class Action {
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    SensitivityDown,
    SensitivityUp,
    Quit,
    Count // not an action: the number of actions (sizes the table)
};

// Action -> physical keys. One action can have several keys (E and Space both move up).
// Defaults are set in the constructor; bind/rebind change them at runtime.
class InputMap {
  public:
    InputMap();

    void bind(Action pAction, int pKey);   // add a key to the action
    void rebind(Action pAction, int pKey); // replace all of the action's keys with this one

    // True if any key bound to the action is currently held
    bool isDown(GLFWwindow* pWindow, Action pAction) const;

  private:
    // A plain constant (not a constexpr member function): inside the class body, a member
    // function's body isn't visible yet, so it can't be used for the array size below.
    static constexpr std::size_t ACTION_COUNT = static_cast<std::size_t>(Action::Count);
    static std::size_t           index(Action pAction) { return static_cast<std::size_t>(pAction); }

    std::array<std::vector<int>, ACTION_COUNT> keys;
};
