#pragma once

#include "../../libs/script.h"

class WarningScreen : public LuaScript {
    sol::protected_function fn_update;
    sol::protected_function fn_draw;
    sol::protected_function fn_is_finished;
public:
    WarningScreen(double current_ms);
    void update(double current_ms);
    void draw();
    bool is_finished();
};
