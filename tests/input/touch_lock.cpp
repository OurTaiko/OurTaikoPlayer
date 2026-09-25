// Exercise the actual SDL callback without starting a window or audio device.
#include "../../src/libs/input.cpp"
#include <cassert>
#include <iostream>

GlobalData global_data;
TextureWrapper tex;
void TextureWrapper::unload_textures() {}

static bool input_locked = false;
bool is_input_locked() { return input_locked; }
int ray::GetScreenWidth() { return 1280; }
int ray::GetScreenHeight() { return 720; }
#ifdef OURTAIKO_PLATFORM_IOS
double ios_game_time_ms() { return 1000.0; }
#endif
#if defined(__linux__) && !defined(PLATFORM_ANDROID)
const bool* SDL_GetKeyboardState(int*) { return nullptr; }
SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode, SDL_Keymod*) { return SDL_SCANCODE_UNKNOWN; }
#endif

static void finger(Uint32 type, SDL_FingerID id) {
    SDL_Event event{};
    event.type = type;
    event.tfinger.fingerID = id;
    event.tfinger.x = 0.45f;
    event.tfinger.y = 0.9f;
    touch_event_watch(nullptr, &event);
}

int main() {
    Config config;
    config.general.touch_input = true;
    global_data.config = &config;

    for (auto release : {SDL_EVENT_FINGER_UP, SDL_EVENT_FINGER_CANCELED}) {
        finger(SDL_EVENT_FINGER_DOWN, 1);
        assert(check_key_pressed(TOUCH_L_DON));
        // A second DOWN for an active finger must not become another hit.
        finger(SDL_EVENT_FINGER_DOWN, 1);
        assert(!check_key_pressed(TOUCH_L_DON));
        input_locked = true;
        clear_input_buffers(); // Scene changes clear queued events only.
        finger(release, 1);
        assert(!touch_drum_pressed.load());
        assert(!check_key_released(TOUCH_L_DON));
        input_locked = false;
        // Platforms can reuse the ID as soon as the previous touch ends.
        finger(SDL_EVENT_FINGER_DOWN, 1);
        assert(check_key_pressed(TOUCH_L_DON));
        finger(SDL_EVENT_FINGER_UP, 1);
        assert(check_key_released(TOUCH_L_DON));
    }

    // Touches starting under the lock must never leak a hit after unlocking.
    input_locked = true;
    finger(SDL_EVENT_FINGER_DOWN, 2);
    input_locked = false;
    finger(SDL_EVENT_FINGER_UP, 2);
    assert(pressed_keys.empty() && released_keys.empty());
    finger(SDL_EVENT_FINGER_DOWN, 2);
    assert(check_key_pressed(TOUCH_L_DON));
    finger(SDL_EVENT_FINGER_UP, 2);
    assert(check_key_released(TOUCH_L_DON));

    // Releasing one finger must preserve the other finger's active state.
    finger(SDL_EVENT_FINGER_DOWN, 3);
    finger(SDL_EVENT_FINGER_DOWN, 4);
    clear_input_buffers();
    input_locked = true;
    config.general.touch_input = false;
    finger(SDL_EVENT_FINGER_CANCELED, 3);
    assert(touch_drum_pressed.load());
    input_locked = false;
    config.general.touch_input = true;
    finger(SDL_EVENT_FINGER_DOWN, 4);
    assert(!check_key_pressed(TOUCH_L_DON));
    config.general.touch_input = false;
    finger(SDL_EVENT_FINGER_UP, 4);
    assert(!touch_drum_pressed.load());
    assert(check_key_released(TOUCH_L_DON));
    config.general.touch_input = true;
    finger(SDL_EVENT_FINGER_DOWN, 3);
    assert(check_key_pressed(TOUCH_L_DON));
    finger(SDL_EVENT_FINGER_UP, 3);
    assert(check_key_released(TOUCH_L_DON));
    assert(touch_id_to_vkey.empty());
    assert(pressed_keys.empty() && released_keys.empty());
    global_data.config = nullptr;
    std::cout << "PASS: locked touch release/cancel, reused IDs, ignored presses, and multiple fingers\n";
}
