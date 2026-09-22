#include "transition.h"
#include "../../libs/global_data.h"
#include <algorithm>
#include <cmath>
#include "../../libs/fanmade.h"

Transition::Transition(const std::string& title, const std::string& subtitle, bool is_second) :
    is_second(is_second) {
    rainbow_up = (MoveAnimation*)global_tex.get_animation(0);
    mini_up = (MoveAnimation*)global_tex.get_animation(1);
    chara_down = (MoveAnimation*)global_tex.get_animation(2);
    song_info_fade = (FadeAnimation*)global_tex.get_animation(3);
    song_info_fade_out = (FadeAnimation*)global_tex.get_animation(4);

    this->title = std::make_unique<OutlinedText>(title, global_tex.skin_config[SC::TRANSITION_TITLE].font_size, ray::WHITE, ray::BLACK, false, 5);
    download_title = title;
    this->subtitle = std::make_unique<OutlinedText>(subtitle, global_tex.skin_config[SC::TRANSITION_SUBTITLE].font_size, ray::WHITE, ray::BLACK, false, 5);

    t_rainbow_text_bg = global_tex.get_texture("rainbow_transition/text_bg");
    t_rainbow_bg_bottom = global_tex.get_texture("rainbow_transition/rainbow_bg_bottom");
    t_rainbow_bg_top = global_tex.get_texture("rainbow_transition/rainbow_bg_top");
    t_rainbow_bg = global_tex.get_texture("rainbow_transition/rainbow_bg");
    t_chara_left = global_tex.get_texture("rainbow_transition/chara_left");
    t_chara_right = global_tex.get_texture("rainbow_transition/chara_right");
    t_chara_center = global_tex.get_texture("rainbow_transition/chara_center");

    if (!load("SongTransition", "transition", title, subtitle, is_second)) return;
    fn_update    = lua_object["update"];
    fn_draw_bg   = lua_object["draw_bg"];
    fn_draw_info = lua_object["draw_info"];
}

Transition::~Transition() {
    *cancel_remote = true;
    if (loading_graphic.has_value()) {
        ray::UnloadTexture(loading_graphic.value());
    }
}

void Transition::add_loading_graphic(const std::string& path) {
    if (loading_graphic.has_value()) {
        ray::UnloadTexture(loading_graphic.value());
        loading_graphic.reset();
    }
    ray::Texture2D tex = ray::LoadTexture(path.c_str());
    if (tex.id == 0) {
        spdlog::error("Failed to load transition loading graphic: {}", path);
        return;
    }
    loading_graphic.emplace(tex);
    ray::GenTextureMipmaps(&loading_graphic.value());
    ray::SetTextureFilter(loading_graphic.value(), ray::TEXTURE_FILTER_TRILINEAR);
}

void Transition::set_dan(int color, const std::string& rank_name) {
    dan_color = color;
    global_tex.load_folder("dan_loading", "loading_dan");
    t_loading_dan_night = global_tex.get_texture("loading_dan/night");
    t_loading_dan_plaque = global_tex.get_texture("loading_dan/plaque");
    if (!rank_name.empty()) {
        dan_rank_text = std::make_unique<OutlinedText>(
            rank_name, global_tex.skin_config[SC::DAN_TITLE].font_size,
            ray::WHITE, ray::BLACK, true);
    }
}

void Transition::draw_dan(float total_offset) {
    const double f = 36.0 + (get_current_ms() - dan_start_ms) * 0.06;
    const float  y = -106.0f - 0.625f * (float)std::clamp(f - 36.0, 0.0, 179.0);
    const float  a = (float)std::clamp((f - 44.0) / 20.0, 0.0, 1.0);
    const float  black = (float)std::clamp(1.0 - (f - 36.0) / 29.0, 0.0, 1.0);
    const float  dy = -(float)rainbow_up->attribute - total_offset;

    global_tex.draw_texture(t_loading_dan_night, {.y = y + dy});
    if (a > 0.0f) {
        global_tex.draw_texture(t_loading_dan_plaque,
                                {.frame = std::clamp(dan_color, 0, 6), .y = dy, .fade = a});
        if (dan_rank_text) {
            const SkinInfo* p = global_tex.skin_entry("dan_loading_rank");
            const float rx = p ? p->x : 1697.0f;
            const float ry = p ? p->y : 290.0f;
            dan_rank_text->draw({.x = rx - dan_rank_text->width / 2.0f,
                                 .y = ry - dan_rank_text->height / 2.0f + dy,
                                 .fade = a});
        }
    }
    if (black > 0.0f)
        ray::DrawRectangle(0, 0, global_tex.screen_width, global_tex.screen_height,
                           ray::Fade(ray::BLACK, black));
}

void Transition::start() {
    if (!is_second) {
        auto pn = global_data.player_num == PlayerNum::TWO_PLAYER ? PlayerNum::P1 : global_data.player_num;
        if (remote_players.empty()) remote_players.push_back((int)pn);
        remote_source = global_data.session_data[remote_players.front()].selected_song;
        if (fanmade::client().chart(remote_source)) {
            remote_download = std::async(std::launch::async, [source=remote_source, cancel=cancel_remote, state=download_state] {
                return fanmade::client().prepare(source, cancel, [state](const fanmade::DownloadProgress& progress) {
                    std::lock_guard lock(state->mutex);
                    state->progress = progress;
                });
            });
        }
    }
    dan_start_ms = get_current_ms();
    rainbow_up->start();
    mini_up->start();
    chara_down->start();
    song_info_fade->start();
    song_info_fade_out->start();
}

void Transition::update(double current_ms) {
    if (remote_download.valid() && remote_download.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto path = remote_download.get();
            auto chart = fanmade::client().chart(path);
            for (int pn : remote_players) {
                const auto& session = global_data.session_data[pn];
                if (session.selected_song != remote_source) continue;
                int diff = session.selected_difficulty;
                if (!chart || diff < 0 || diff >= 5 || !chart->difficulties[diff])
                    throw std::runtime_error("DIFFICULTY_REMOVED_BY_AUTHOR");
            }
            for (int pn : remote_players) {
                auto& session = global_data.session_data[pn];
                if (session.selected_song != remote_source) continue;
                session.selected_song = path;
                session.song_hash = "fanmade:" + chart->server + ":" + chart->id + ":" + chart->version + ":" + std::to_string(session.selected_difficulty);
            }
        } catch (const std::exception& e) { download_error = e.what(); }
    }
    call(fn_update, "SongTransition:update", current_ms,
         (double)rainbow_up->attribute, (double)song_info_fade->attribute);
    rainbow_up->update(current_ms);
    chara_down->update(current_ms);
    mini_up->update(current_ms);
    song_info_fade->update(current_ms);
    song_info_fade_out->update(current_ms);
}

bool Transition::is_finished() {
    return song_info_fade->is_finished && !remote_download.valid() && download_error.empty();
}

void Transition::draw_song_info() {
    float fade_1 = song_info_fade->attribute;
    float fade_2 = std::min(0.70, song_info_fade->attribute);
    float offset = 0;
    if (is_second) {
        fade_1 = song_info_fade_out->attribute;
        fade_2 = std::min(0.70, song_info_fade_out->attribute);
        offset = global_tex.skin_config[SC::TRANSITION_OFFSET].y - rainbow_up->attribute;
    }
    global_tex.draw_texture(t_rainbow_text_bg, {.y=(float)-rainbow_up->attribute - offset, .fade=fade_2});

    float x = (float)global_tex.screen_width/2 - title->width/2;
    float y = global_tex.skin_config[SC::TRANSITION_TITLE].y - title->height/2 - rainbow_up->attribute - offset;
    title->draw({.x = x, .y = y, .fade = fade_1});

    x = (float)global_tex.screen_width/2 - subtitle->width/2;
    y = global_tex.skin_config[SC::TRANSITION_SUBTITLE].y - subtitle->height/2 - rainbow_up->attribute - offset;
    subtitle->draw({.x = x, .y = y, .fade = fade_1});
}

void Transition::draw_default(float total_offset) {
    global_tex.draw_texture(t_rainbow_bg_bottom, {.y=(float)-rainbow_up->attribute - total_offset});
    global_tex.draw_texture(t_rainbow_bg_top, {.y=(float)-rainbow_up->attribute - total_offset});
    global_tex.draw_texture(t_rainbow_bg, {.y=(float)-rainbow_up->attribute - total_offset});
    float offset = chara_down->attribute;
    float chara_offset = 0;
    if (is_second) {
        offset = chara_down->attribute - mini_up->attribute/3;
        chara_offset = global_tex.skin_config[SC::TRANSITION_CHARA_OFFSET].y;
    }
    global_tex.draw_texture(t_chara_left, {.x=(float)-mini_up->attribute/2 - chara_offset, .y=(float)-mini_up->attribute + offset - total_offset});
    global_tex.draw_texture(t_chara_right, {.x=(float)mini_up->attribute/2 + chara_offset, .y=(float)-mini_up->attribute + offset - total_offset});
    global_tex.draw_texture(t_chara_center, {.y=(float)-rainbow_up->attribute + offset - total_offset});
}

void Transition::draw_download() {
    using File = fanmade::FileProgress;
    using Stage = fanmade::DownloadProgress::Stage;
    fanmade::DownloadProgress progress;
    { std::lock_guard lock(download_state->mutex); progress = download_state->progress; }
    const bool zh = global_data.config->general.language.rfind("zh", 0) == 0;
    const auto label = [zh](const char* en, const char* cn) { return std::string(zh ? cn : en); };
    const auto size_text = [](uint64_t bytes) {
        if (bytes >= 1024 * 1024) return fmt::format("{:.1f} MB", bytes / (1024.0 * 1024.0));
        if (bytes >= 1024) return fmt::format("{:.1f} KB", bytes / 1024.0);
        return fmt::format("{} B", bytes);
    };
    const auto detail = [&](const File& file) {
        switch (file.state) {
            case File::State::Waiting: return label("Waiting", "等待下载");
            case File::State::Cached: return label("Cached", "已缓存") + "  ·  " + size_text(file.total);
            case File::State::Complete: return label("Complete", "已完成") + "  ·  " + size_text(file.total);
            case File::State::Verifying: return label("Verifying", "校验中");
            case File::State::Downloading:
                if (!file.total) return size_text(file.received) + "  ·  " + label("Downloading", "下载中");
                return fmt::format("{}%  ·  {} / {}", static_cast<int>(100.0 * std::min(file.received, file.total) / file.total),
                                   size_text(file.received), size_text(file.total));
        }
        return std::string{};
    };
    const std::string heading = !download_error.empty() ? label("Download failed", "下载失败")
                              : *cancel_remote ? label("Cancelling download…", "正在取消下载…")
                              : label("Preparing song", "正在准备歌曲");
    std::string footer = label("Back: cancel download", "返回：取消下载");
    if (!download_error.empty()) footer = download_error + "  ·  " + label("Back: return", "返回：回到选曲");
    else if (progress.stage == Stage::Checking) footer = label("Checking song details…", "正在获取歌曲信息…") + "  ·  " + footer;
    else if (progress.stage == Stage::Preparing) footer = label("Preparing chart…", "正在准备谱面…");
    else if (progress.stage == Stage::Ready) footer = label("Ready", "准备完成");
    const std::string chart_label = label("Chart", "谱面");
    const std::string audio_label = label("Song audio", "歌曲音频");
    const std::string chart_detail = detail(progress.chart), audio_detail = detail(progress.audio);

    const float scale = global_tex.screen_width / 1280.0f;
    const float width = std::min(900.0f * scale, global_tex.screen_width - 80.0f * scale);
    const float height = 350.0f * scale;
    const float x = (global_tex.screen_width - width) / 2.0f;
    // Keep the file progress above the global touch drum at the bottom.
    const float y = 40.0f * scale;
    const float inset = 36 * scale, content_width = width - 2 * inset;
    const ray::Color background{12, 17, 28, 255}, panel{25, 33, 49, 255};
    const ray::Color muted{166, 181, 202, 255}, accent{93, 211, 199, 255};
    ray::DrawRectangle(0, 0, global_tex.screen_width, global_tex.screen_height, background);
    ray::DrawRectangleRec({x, y, width, height}, panel);

    // Request all glyphs together before drawing so an atlas rebuild cannot
    // invalidate a font already in use. Numeric progress uses cached glyphs.
    const int font_size = std::max(16, static_cast<int>(24 * scale));
    auto font = font_manager.get_font(heading + download_title + footer + chart_label + audio_label
                                    + chart_detail + audio_detail + "0123456789.% /BKM·", font_size);
    auto draw_text = [&](const std::string& text, float tx, float ty, float size, ray::Color color, bool right = false) {
        float measured = ray::MeasureTextEx(font, text.c_str(), size, scale).x;
        if (measured > content_width) { size *= content_width / measured; measured = content_width; }
        ray::DrawTextEx(font, text.c_str(), {tx - (right ? measured : 0), ty}, size, scale, color);
    };
    draw_text(heading, x + inset, y + 28 * scale, 32 * scale, ray::WHITE);
    draw_text(download_title, x + inset, y + 78 * scale, 24 * scale, muted);
    auto row = [&](const std::string& name, const std::string& text, const File& file, float top) {
        draw_text(name, x + inset, top, 24 * scale, ray::WHITE);
        draw_text(text, x + width - inset, top + 2 * scale, 20 * scale, muted, true);
        const float bar_y = top + 40 * scale, bar_height = 8 * scale;
        ray::DrawRectangleRec({x + inset, bar_y, content_width, bar_height}, {49, 62, 82, 255});
        if (file.state == File::State::Waiting) return;
        if (file.state == File::State::Downloading && !file.total) {
            float position = static_cast<float>(std::fmod(get_current_ms() / 1200.0, 1.0));
            ray::DrawRectangleRec({x + inset + position * content_width * 0.75f, bar_y, content_width * 0.25f, bar_height}, accent);
        } else {
            float fraction = file.total ? static_cast<float>(std::min(file.received, file.total)) / file.total : 1.0f;
            ray::DrawRectangleRec({x + inset, bar_y, content_width * fraction, bar_height}, accent);
        }
    };
    row(chart_label, chart_detail, progress.chart, y + 125 * scale);
    row(audio_label, audio_detail, progress.audio, y + 210 * scale);
    draw_text(footer, x + inset, y + 297 * scale, 20 * scale, muted);
}

void Transition::draw() {
    if (remote_download.valid() || !download_error.empty()) {
        draw_download();
        return;
    }
    float total_offset = 0;
    if (is_second) total_offset = global_tex.skin_config[SC::TRANSITION_OFFSET].y;
    if (dan_color >= 0) {
        draw_dan(total_offset);
        return;
    }

    const bool scripted_bg = fn_draw_bg.valid();
    if (scripted_bg) {
        call(fn_draw_bg, "SongTransition:draw_bg", total_offset,
             (double)rainbow_up->attribute, (double)mini_up->attribute,
             (double)chara_down->attribute);
        if (!loading_graphic.has_value()) {
            if (fn_draw_info.valid()) {
                call(fn_draw_info, "SongTransition:draw_info", total_offset,
                     (double)rainbow_up->attribute, (double)song_info_fade->attribute,
                     (double)song_info_fade_out->attribute);
            } else {
                draw_song_info();
            }
            return;
        }
    }
    if (loading_graphic.has_value()) {
        ray::Rectangle src = {0, 0, (float)loading_graphic.value().width, (float)loading_graphic.value().height};
        ray::Rectangle dst = {0, global_tex.screen_height + (global_tex.skin_config[SC::TRANSITION_OFFSET].y - global_tex.screen_height) - (float)rainbow_up->attribute - total_offset, (float)global_tex.screen_width, (float)global_tex.screen_height};
        ray::DrawTexturePro(loading_graphic.value(), src, dst, {0,0}, 0, ray::WHITE);
    } else {
        draw_default(total_offset);
    }

    if (fn_draw_info.valid()) {
        call(fn_draw_info, "SongTransition:draw_info", total_offset,
             (double)rainbow_up->attribute, (double)song_info_fade->attribute,
             (double)song_info_fade_out->attribute);
    } else {
        draw_song_info();
    }
}
