#include "../../src/libs/fanmade.h"
#include "../../src/libs/subtitle_rotation.h"
#include "../../src/objects/enums.h"
#include <chrono>
#include <future>
#include "../../src/libs/parsers/tja.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#include <stdexcept>
using namespace fanmade;
void check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
std::string read_file(const fs::path& path) { std::ifstream f(path); return {std::istreambuf_iterator<char>(f),{}}; }
void parser_tests() {
    check(sha256("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA-256 vector");
    check(!show_maker_credit(true,true,0) && !show_maker_credit(true,true,2999),"subtitle gets first three seconds");
    check(show_maker_credit(true,true,3000) && show_maker_credit(true,true,5999),"maker gets next three seconds");
    check(!show_maker_credit(true,true,6000) && show_maker_credit(true,true,9000),"rotation repeats");
    check(show_maker_credit(false,true,0) && show_maker_credit(false,true,6000),"maker-only never goes blank");
    check(!show_maker_credit(true,false,3000) && !show_maker_credit(false,false,3000),"missing maker has no blank slot");
    Chart c; c.maker="A | B"; c.audio_name="fixture.ogg"; c.titles["en"]="Renamed"; c.subtitles["zh"]="中文副标题";
    c.difficulties[3]=Difficulty{"Oni",8,2,true,""};
    auto tja=playable_tja("TITLE:Original\nMAKER:Old\nBPM:120\nWAVE:../external.ogg\nBGMOVIE:/secret.mp4\nCOURSE:Oni\nLEVEL:8\nSTYLE:Double\n#START P1\n1111,\n#END\n#START P2\n2222,\n#END\nCOURSE:Oni\nLEVEL:8\nBALLOON:3,4\n#START\n1234,\n#END\n",c);
    check(tja.find("1234,")!=std::string::npos && tja.find("1111,")==std::string::npos && tja.find("2222,")==std::string::npos,"select exact single block");
    check(tja.find("BALLOON:3,4")!=std::string::npos && tja.find("BGMOVIE")==std::string::npos && tja.find("../external")==std::string::npos,"gameplay headers and confined assets");
    check(tja.find("TITLE:Renamed")!=std::string::npos && tja.find("SUBTITLEZH:中文副标题")!=std::string::npos,"metadata overrides");
    check(tja.find("MAKER:A | B\n")!=std::string::npos && tja.find("MAKER:Old")==std::string::npos,"aggregate maker overrides original");
    check(tja.find("WAVE:audio.ogg\n")!=std::string::npos,"OGG cache reference");
    c.audio_name="../Mistletoe.MP3";
    auto mp3=playable_tja("COURSE:Oni\n#START\n1,\n#END\nCOURSE:Oni\n#START\n2,\n#END\nCOURSE:Oni\n#START\n3,\n#END\n",c);
    check(mp3.find("WAVE:audio.mp3\n")!=std::string::npos && mp3.find("../")==std::string::npos,"MP3 case normalization and confined cache reference");
    c.audio_name="bad.wav";
    bool unsupported=false; try { playable_tja("",c); } catch(...) { unsupported=true; }
    check(unsupported,"unsupported audio suffix rejected");
    c.audio_name="fixture.ogg";
    c.difficulties[3]=Difficulty{"Oni",8,0,false,"P1"};
    c.blocks={*c.difficulties[3],Difficulty{"Oni",8,1,false,"P2"}};
    auto dual=playable_tja("COURSE:Oni\nLEVEL:8\n#START P1\n1111,\n#END\n#START P2\n2222,\n#END\n",c);
    check(dual.find("#START P1")!=std::string::npos && dual.find("#START P2")!=std::string::npos,"retain double lanes");
    c.difficulties[3]->block_index=7;
    bool rejected=false; try { playable_tja("COURSE:Oni\n#START\n1,\n#END\n",c); } catch(...) { rejected=true; }
    check(rejected,"reject missing block");
}
void load_servers(Client& client) {
    for(const auto& server:fs::directory_iterator(client.song_paths({}).front())) {
        check(client.is_server(server.path()),"registered server");
        check(client.load_directory(server.path()),"server loads all categories");
        for(const auto& entry:fs::directory_iterator(server.path())) if(fs::is_directory(entry.path()))
            check(!client.load_directory(entry.path()),"category navigation never requests HTTP");
    }
}
void guest_tests(const std::string& base,const fs::path& cache) {
    std::vector<ServerConfig> guests{
        {"Guest",base+"/guest","","",""},
        {"No password",base+"/guest","fixture","",""},
        {"No username",base+"/guest","","fixture-password",""},
        {"Wrong password",base+"/guest","fixture","wrong",""}
    };
    for(size_t i=0;i<guests.size();++i) {
        auto dir=cache/std::to_string(i);
        Client client;
        client.bootstrap({guests[i]},dir);
        check(client.online(),"guest connects to catalog");
        check(client.status().find("guest; scores disabled")!=std::string::npos,"guest mode visible");
        if(i==3) check(client.status().find("login failed: HTTP_401")!=std::string::npos,"failed login remains visible");
        load_servers(client);
        auto server=fs::directory_iterator(client.song_paths({}).front())->path();
        auto path=server/"game"/(std::string(32,'1')+".tja");
        check(client.chart(path).has_value() && !client.best(path,3),"guest catalog has no personal scores");
        auto playable=client.prepare(path);
        TJAParser parsed(playable);
        auto [notes,normal,expert,master]=parsed.notes_to_position(3);
        check(!notes.notes.empty() && fs::exists(parsed.metadata.wave),"guest downloads playable chart and audio");
        Score score; score.score=999999;
        client.submit(playable,3,score);
        client.update();
        check(!fs::exists(dir/"pending") && !client.best(path,3),"guest scores never enter upload queue");
    }
    // The same username with its password removed must not expose cached
    // account scores or replay that account's durable upload queue.
    auto dir=cache/"logout";
    ServerConfig config{"Logout",base+"/guest","fixture","fixture-password",""};
    fs::path path;
    {
        Client client; client.bootstrap({config},dir); load_servers(client);
        auto server=fs::directory_iterator(client.song_paths({}).front())->path();
        path=server/"game"/(std::string(32,'1')+".tja");
        check(client.best(path,3).has_value(),"authenticated account loads scores");
        auto guest_config=config; guest_config.password.clear();
        client.bootstrap({guest_config},dir); load_servers(client);
        check(!client.best(path,3),"switching to guest clears in-memory account scores");
    }
    auto pending=dir/"pending"/sha256(config.base_url+"\n"+config.username)/"existing.json";
    fs::create_directories(pending.parent_path());
    { std::ofstream file(pending); file<<"retained account upload"; }
    config.password.clear();
    {
        Client guest; guest.bootstrap({config},dir); load_servers(guest);
        check(!guest.best(path,3),"guest bootstrap clears account score display");
        guest.submit(path,3,Score{});
        guest.update();
    }
    check(read_file(pending)=="retained account upload","guest leaves account queue untouched");
    check(std::distance(fs::directory_iterator(pending.parent_path()),fs::directory_iterator{})==1,"guest does not append deferred uploads");
}
void refresh_tests(const std::string& base,const fs::path& cache) {
    Client client;
    std::vector<ServerConfig> config{{"Refresh",base+"/refresh","fixture","fixture-password",""}};
    // Slow bootstrap must not block frame-side update() behind its pump mutex.
    auto job=std::async(std::launch::async,[&]{client.bootstrap(config,cache);});
    int frames=0;
    while(job.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready) {
        auto start=std::chrono::steady_clock::now(); client.update();
        check(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(100),"refresh does not block frame update");
        ++frames; std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    job.get(); check(frames>5,"slow refresh exercised frame loop");
    auto root=client.song_paths({}).front();
    auto server=fs::directory_iterator(root)->path();
    auto game=server/"game",pop=server/"pop",anime=server/"anime";
    check(client.folder_count(server)==1 && client.folder_count(game)==1,"initial counts from metadata");
    auto old=pop/(std::string(32,'1')+".tja");
    client.load_directory(server);
    check(client.folder_count(server)==1 && client.folder_count(game)==1 && client.folder_count(pop)==1,"multi-category server total deduplicated");
    check(fs::exists(old),"first snapshot visible");
    auto old_anime=anime/old.filename();
    check(client.folder_count(anime)==1 && fs::exists(old_anime),"Anime membership loaded");
    bool failed=false;
    try {client.load_directory(server);} catch(...) {failed=true;}
    check(failed && fs::exists(old) && client.chart(old).has_value(),"failed refresh preserves complete old snapshot");
    check(client.folder_count(game)==1 && client.folder_count(server)==1,"failed refresh preserves counts");
    check(client.folder_count(anime)==1 && fs::exists(old_anime) && client.chart(old_anime).has_value(),"failed refresh preserves Anime snapshot");
    client.load_directory(server);
    check(!fs::exists(old) && !client.chart(old),"removed memberships disappear from disk and registry");
    check(client.folder_count(game)==2 && client.folder_count(pop)==0 && client.folder_count(server)==2,"reopening updates every count including zero");
    check(client.folder_count(anime)==0 && !fs::exists(old_anime) && !client.chart(old_anime),"removed Anime membership resets count and catalog");
    client.bootstrap(config,cache);
    check(client.is_category(server/"classic"),"reentering mode refreshes category list");
    for(const auto& file:fs::recursive_directory_iterator(root)) check(file.path().extension()!=".tja","mode refresh does not preload chart lists");
}
int main(int argc,char** argv) {
 try {
    parser_tests();
    if(argc<3) { std::cout<<"parser checks passed\n"; return 0; }
    std::string base=argv[1]; fs::path cache=argv[2];
    bool real=argc>3;
    std::string username=real?std::getenv("FANMADE_TEST_USERNAME"):"fixture";
    std::string password=real?std::getenv("FANMADE_TEST_PASSWORD"):"fixture-password";
    std::vector<ServerConfig> configs{{"OurTaiko Fanmade",base,username,password,""}};
    if(!real) configs.push_back({"Second server",base+"/second",username,password,base});
    if(!real) {
        for(const auto* variant : {"missing-combo", "null-combo"}) {
            Client invalid;
            invalid.bootstrap({{"Invalid score",base+"/"+variant,username,password,""}},cache/variant);
            check(!invalid.online()&&invalid.status().find("API_NUMBER_INVALID")!=std::string::npos,"reject missing or null maximum combo");
        }
    }
    if(!real) refresh_tests(base,cache/"refresh-tests");
    if(!real) guest_tests(base,cache/"guest-tests");
    Client client; client.bootstrap(configs,cache);
    check(client.online(),client.status().c_str());
    auto roots=client.song_paths({}); check(roots.size()==1,"catalog root");
    for(auto& f:fs::recursive_directory_iterator(roots[0])) check(f.path().extension()!=".tja","bootstrap must not materialize any charts");
    check(!client.load_directory(roots[0]),"root listing does not fetch charts");
    if(!real) for(const auto& server:fs::directory_iterator(roots[0])) {
        auto anime=server.path()/"anime";
        check(client.is_category(anime) && client.folder_count(anime)==1,"API Anime category registered with count");
        auto box=read_file(anime/"box.def");
        check(box.find("#TITLE:Anime\n")!=std::string::npos,"API Anime title reaches folder");
        auto start=box.find("#GENRE:");
        check(start!=std::string::npos,"API Anime genre reaches folder");
        start+=7;
        auto genre=box.substr(start,box.find('\n',start)-start);
        check(get_genre_index(genre)==GenreIndex::ANIME,"API genre resolves to native Anime style");
        check(genre_to_ref_frame(get_genre_index(genre))==2,"Anime uses its skin frame");
    }
    load_servers(client);
    std::vector<fs::path> paths;
    for(auto& f:fs::recursive_directory_iterator(roots[0])) if(f.path().extension()==".tja" && (real||f.path().parent_path().filename()=="game")) paths.push_back(f.path());
    check(paths.size()>=(real?1u:2u),"all server catalogs");
    for(auto& path:paths) {
        auto chart=client.chart(path); check(chart.has_value(),"registered chart");
        if(!real) {
            check(client.best(path,3)->score==(chart->title=="Second"?700000:900000),"server score isolation");
            check(client.best(path,3)->max_combo==(chart->title=="Second"?6:8),"server maximum combo isolation");
        }
    }
    auto path=paths.front();
    if(!real) {
        for(const auto& candidate:paths) {
            if(client.chart(candidate)->title=="First") path=candidate;
            else {
                bool unknown_length_progress=false;
                auto ogg=client.prepare(candidate,{},[&](const DownloadProgress& p) {
                    if(p.audio.state==FileProgress::State::Downloading && p.audio.received>0 && p.audio.total==0)
                        unknown_length_progress=true;
                });
                check(unknown_length_progress,"report received bytes without fabricating an unknown total");
                TJAParser parsed_ogg(ogg);
                check(parsed_ogg.metadata.wave.filename()=="audio.ogg" && fs::exists(parsed_ogg.metadata.wave),"OGG audio reference remains playable");
            }
        }
    }
    auto cancelled=std::make_shared<std::atomic_bool>(true);
    bool cancellation_worked=false;
    try { client.prepare(path,cancelled); } catch(const std::exception&) { cancellation_worked=true; }
    check(cancellation_worked,"cancelled download must not start");
    std::vector<DownloadProgress> progress;
    auto playable=client.prepare(path,{},[&](const DownloadProgress& p) { progress.push_back(p); });
    check(!progress.empty() && progress.front().stage==DownloadProgress::Stage::Checking
          && progress.back().stage==DownloadProgress::Stage::Ready,"preparation stage lifecycle");
    for(bool chart_file : {true,false}) {
        bool partial=false, verifying=false, complete=false;
        uint64_t received=0;
        for(const auto& p:progress) {
            const auto& file=chart_file?p.chart:p.audio;
            if(file.state==FileProgress::State::Downloading) {
                check(file.received>=received,"download byte progress is monotonic");
                received=file.received;
                if(file.total && file.received>0 && file.received<file.total) partial=true;
            }
            if(file.state==FileProgress::State::Verifying) verifying=true;
            if(file.state==FileProgress::State::Complete) { check(verifying,"completion follows verification"); complete=true; }
        }
        check(complete,"both files complete");
        if(!real) check(partial,"both files publish intermediate byte progress");
    }
    check(fs::exists(playable),"TJA ready");
    if (!real) {
        TJAParser credits(playable);
        check(credits.metadata.maker=="A | B", "API aggregate reaches native parser");
        TJAParser catalog_credits(path);
        check(catalog_credits.metadata.maker=="A | B", "API aggregate reaches song select catalog");
    }
    if(!real) {
        auto pop=path.parent_path().parent_path()/"pop";
        check(!client.load_directory(pop),"category uses server snapshot");
        auto same=pop/path.filename();
        check(client.chart(same)->id==client.chart(path)->id,"one chart belongs to multiple categories");
        check(client.best(same,3)->score==client.best(path,3)->score,"cross-category score identity");
        check(client.prepare(same)==playable,"cross-category download cache identity");
        auto anime=path.parent_path().parent_path()/"anime"/path.filename();
        check(client.chart(anime)->id==client.chart(path)->id,"Anime chart loaded from category API");
        check(client.best(anime,3)->score==client.best(path,3)->score,"Anime shares chart score identity");
        check(client.prepare(anime)==playable,"Anime shares download cache identity");
    }
    TJAParser parsed(playable);
    const auto audio_path=parsed.metadata.wave;
    check(fs::exists(audio_path),"parsed TJA resolves downloaded audio");
    if(!real) check(audio_path.filename()=="audio.mp3","MP3 audio keeps decoder-compatible suffix");
    for(int i=0;i<5;i++) if(client.chart(path)->difficulties[i]) {
        auto [notes, normal, expert, master] = parsed.notes_to_position(i);
        check(!notes.notes.empty(), "actual game parser produced empty notes");
    }
    auto first=fs::last_write_time(audio_path);
    DownloadProgress cached;
    client.prepare(path,{},[&](const DownloadProgress& p) {
        check(p.chart.state!=FileProgress::State::Downloading && p.audio.state!=FileProgress::State::Downloading,"cache hit never reports a download");
        cached=p;
    });
    check(cached.chart.state==FileProgress::State::Cached && cached.audio.state==FileProgress::State::Cached,"cache hit reported for both files");
    check(fs::last_write_time(audio_path)==first,"hash hit avoids download");
    if(!real) {
        fs::rename(audio_path,playable.parent_path()/"audio.ogg");
        client.prepare(path);
        check(fs::last_write_time(audio_path)==first && !fs::exists(playable.parent_path()/"audio.ogg"),"verified old MP3 cache renamed without download");
    }
    { std::ofstream file(audio_path,std::ios::trunc); file<<"corrupted"; }
    client.prepare(path);
    check(sha256(read_file(audio_path))==client.chart(path)->audio_hash,"corruption repaired");
    if(!real) {
        Client interrupted;
        interrupted.bootstrap({configs.front()},cache/"cancel-progress");
        load_servers(interrupted);
        fs::path selected;
        for(auto& f:fs::recursive_directory_iterator(interrupted.song_paths({}).front()))
            if(f.path().extension()==".tja") selected=f.path();
        auto cancel=std::make_shared<std::atomic_bool>(false);
        bool rejected=false, finished=false;
        try {
            interrupted.prepare(selected,cancel,[&](const DownloadProgress& p) {
                if(p.audio.state==FileProgress::State::Downloading && p.audio.received>0) *cancel=true;
                if(p.audio.state==FileProgress::State::Complete || p.stage==DownloadProgress::Stage::Ready) finished=true;
            });
        } catch(const std::exception&) { rejected=true; }
        check(*cancel && rejected && !finished,"cancellation still interrupts transfer with progress enabled");
        for(auto& f:fs::recursive_directory_iterator(cache/"cancel-progress"))
            check(f.path().filename()!="audio.mp3" && f.path().filename()!="play.tja","cancelled download never becomes playable");
    }
    auto chart=client.chart(path); int diff=3;
    if(real) {
        diff=-1; for(int i=0;i<5;i++) if(chart->difficulties[i]&&chart->difficulties[i]->cloud) { diff=i; break; }
        check(diff>=0,"single difficulty available");
    }
    Score score; score.good=12; score.ok=3; score.bad=1; score.score=999999; score.drumroll=9; score.max_combo=11;
    client.submit(playable,diff,score);
    for(int n=0;n<40;n++) {
        client.update(); auto best=client.best(path,diff);
        if(best&&best->score==score.score) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    if (!real && (!client.best(path,diff) || client.best(path,diff)->score!=score.score)) {
        // Fixture commits the first request but returns 500. A new client must
        // replay the durable queue with the original key and recover one score.
        Client resumed; resumed.bootstrap(configs,cache);
        load_servers(resumed);
        for(int n=0;n<100;n++) {
            resumed.update(); auto best=resumed.best(path,diff);
            if(best&&best->score==score.score) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        check(resumed.best(path,diff)&&resumed.best(path,diff)->score==score.score&&resumed.best(path,diff)->max_combo==11,"durable idempotent retry preserves maximum combo after restart");
    } else {
        check(client.best(path,diff)&&client.best(path,diff)->score==score.score&&client.best(path,diff)->max_combo==11,client.status().c_str());
    }
    if(!real) {
        Score double_score=score; client.submit(playable,2,double_score);
        check(!client.best(path,2),"double scores excluded");
    }
    std::cout<<"PASS: catalog, isolated scores, download, cache hit, corruption recovery, score and maximum combo submission\n";
 } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
