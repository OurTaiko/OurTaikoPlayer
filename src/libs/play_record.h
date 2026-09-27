#pragma once

#include <cstddef>
#include <utility>
#include <vector>

enum class InputLogType {
    KAT_L = 0,
    DON_L = 1,
    DON_R = 2,
    KAT_R = 3
};

// Keep consumption order, including every hit consumed at the same game time.
// Time is the ms_from_start passed to check_note, not a wall-clock timestamp.
using InputLog = std::vector<std::pair<double, InputLogType>>;

namespace fanmade {
inline constexpr std::size_t max_replay_inputs = 100000;
inline constexpr double max_replay_time_ms = 86400000.0;

struct PlayRecord {
    int audio_offset_ms = 0;
    int visual_offset_ms = 0;
    InputLog inputs;
};
} // namespace fanmade
