#pragma once

#include "play_record.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fanmade {
namespace fs = std::filesystem;
struct ServerConfig {
    std::string name, base_url, username, password, http_proxy;
};
struct Difficulty {
    std::string course;
    int level = 0, block_index = 0;
    bool cloud = false;
    std::string player;
};
struct Chart {
    std::string server, id, version, title, subtitle, maker, tja_hash, audio_hash, encoding, audio_name;
    std::map<std::string, std::string> titles, subtitles;
    double bpm = 120, demo_start = 0;
    std::array<std::optional<Difficulty>, 5> difficulties;
    std::vector<Difficulty> blocks;
};
struct Score {
    std::string id, song, version, difficulty;
    int64_t good = 0, ok = 0, bad = 0, score = 0, drumroll = 0, max_combo = 0;
};
struct FileProgress {
    enum class State { Waiting, Downloading, Verifying, Cached, Complete };
    State state = State::Waiting;
    uint64_t received = 0;
    uint64_t total = 0; // Zero means the server has not supplied a length.
};
struct DownloadProgress {
    enum class Stage { Checking, Files, Preparing, Ready };
    Stage stage = Stage::Checking;
    FileProgress chart, audio;
};
using DownloadCallback = std::function<void(const DownloadProgress&)>;
// No rendering or game globals: the HTTP/cache client can be integration-tested
// against a fixture server without starting raylib or accessing local scores.db.
class Client {
public:
    Client();
    ~Client();
    void bootstrap(const std::vector<ServerConfig>& servers, const fs::path& cache);
    std::vector<fs::path> song_paths(std::vector<fs::path> local) const;
    bool is_category(const fs::path& path) const;
    bool is_server(const fs::path& path) const;
    // -1 means this server has not supplied a count yet; nullopt is a local path.
    std::optional<int> folder_count(const fs::path& path) const;
    // Refresh every category atomically when a server folder opens. Category
    // folders are already materialized and never make HTTP requests.
    bool load_directory(const fs::path& path);
    std::optional<Chart> chart(const fs::path& path) const;
    std::optional<Score> best(const fs::path& path, int difficulty) const;
    // Callback runs on the worker thread; publish a snapshot before rendering.
    fs::path prepare(const fs::path& path, std::shared_ptr<std::atomic_bool> cancel = {}, DownloadCallback progress = {});
    void submit(const fs::path& path, int difficulty, const Score& score,
                const std::optional<PlayRecord>& replay = std::nullopt);
    void update(); // launches queued submissions; never waits for HTTP
    bool online() const;
    uint64_t revision() const;
    std::string status() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
Client& client();
std::string sha256(const std::string& bytes);
// Select the server-identified blocks and normalize only the playable copy.
// Original downloads remain byte-for-byte intact for hash verification.
std::string playable_tja(const std::string& utf8, const Chart& chart);
} // namespace fanmade
