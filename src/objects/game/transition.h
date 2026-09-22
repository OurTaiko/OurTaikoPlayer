#pragma once

#include "../../libs/text.h"
#include <future>
#include <atomic>
#include <filesystem>
#include "../../libs/script.h"
#include "../../libs/fanmade.h"
#include <mutex>

class Transition : public LuaScript {
private:
    sol::protected_function fn_update, fn_draw_bg, fn_draw_info;

    int  dan_color   = -1;
    double dan_start_ms = 0.0;
    std::unique_ptr<OutlinedText> dan_rank_text;
    void draw_dan(float total_offset);

    std::shared_ptr<std::atomic_bool> cancel_remote = std::make_shared<std::atomic_bool>(false);
    struct DownloadState {
        std::mutex mutex;
        fanmade::DownloadProgress progress;
    };
    std::shared_ptr<DownloadState> download_state = std::make_shared<DownloadState>();
    std::string download_title;
    void draw_download();
    std::future<std::filesystem::path> remote_download;
    std::string download_error;
    std::filesystem::path remote_source;
    std::vector<int> remote_players;
    bool is_second;
    std::unique_ptr<OutlinedText> title;
    std::unique_ptr<OutlinedText> subtitle;
    std::optional<ray::Texture2D> loading_graphic;
    void draw_song_info();
    void draw_default(float total_offset);

    MoveAnimation* rainbow_up;
    MoveAnimation* mini_up;
    MoveAnimation* chara_down;
    FadeAnimation* song_info_fade;
    FadeAnimation* song_info_fade_out;

    // "global" screen assets, loaded once at app startup - safe to resolve here.
    TextureObject* t_rainbow_text_bg = nullptr;
    TextureObject* t_rainbow_bg_bottom = nullptr;
    TextureObject* t_rainbow_bg_top = nullptr;
    TextureObject* t_rainbow_bg = nullptr;
    TextureObject* t_chara_left = nullptr;
    TextureObject* t_chara_right = nullptr;
    TextureObject* t_chara_center = nullptr;
    // Resolved in set_dan(), right after it explicitly loads the "loading_dan" folder.
    TextureObject* t_loading_dan_night = nullptr;
    TextureObject* t_loading_dan_plaque = nullptr;
public:

    Transition(const std::string& title, const std::string& subtitle, bool is_second);
    ~Transition();
    void start();
    void set_remote_players(std::vector<int> players) { remote_players = std::move(players); }
    void add_loading_graphic(const std::string& path);
    void set_dan(int color, const std::string& rank_name);
    void update(double current_ms);
    void draw();

    bool is_finished();
    bool loading() const { return remote_download.valid(); }
    void cancel_download() { *cancel_remote = true; }
    bool cancelled() const { return *cancel_remote && !loading(); }
    const std::string& error() const { return download_error; }
};
