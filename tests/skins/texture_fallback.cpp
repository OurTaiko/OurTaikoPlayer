#include "../../src/libs/global_data.h"
#include "../../src/objects/game/exam_caption.h"
#include <cassert>
#include <iostream>

GlobalData global_data;
static void load(const std::string& id) {
    tex.textures[id] = std::make_shared<TextureObject>("fixture", 1, 1);
}
int main() {
    Config config;
    config.general.language = "zh";
    global_data.config = &config;
    assert(!tex.has_texture("combo/combo_zh"));
    load("combo/combo_en");
    assert(tex.has_texture("combo/combo_zh"));
    assert(tex.get_texture("combo/combo_zh") == tex.get_texture("combo/combo_en"));
    load("combo/combo_ja");
    assert(tex.get_texture("combo/combo_zh") == tex.get_texture("combo/combo_ja"));
    config.general.language = "en";
    assert(tex.get_texture("combo/combo_en") == tex.get_texture("combo/combo_en"));
    tex.textures.erase("combo/combo_en");
    assert(tex.get_texture("combo/combo_en") == tex.get_texture("combo/combo_ja"));
    assert(!tex.has_texture("combo/unrelated_name"));
    assert(tex.language_variants("combo/unrelated_name") == std::vector<std::string>{"combo/unrelated_name"});
    config.general.language = "zh_tw";
    assert(tex.get_texture("combo/combo_zh_tw") == tex.get_texture("combo/combo_ja"));
    load("dan_info/exam_drumroll");
    assert(exam_icon_id("dan_info/exam_roll", "dan_info") == tex.get_texture("dan_info/exam_drumroll"));
    assert(exam_icon_id("dan_info/exam_gauge", "dan_info") == tex.get_texture("dan_info/exam_gauge"));
    load("dan_info/exam_roll");
    assert(exam_icon_id("dan_info/exam_roll", "dan_info") == tex.get_texture("dan_info/exam_roll"));
    tex.unload_textures();
    assert(!tex.has_texture("combo/combo_zh_tw"));
    global_data.config = nullptr;
    std::cout << "PASS: loaded texture fallback, locale order, skin unload, drumroll-only substitution\n";
}
