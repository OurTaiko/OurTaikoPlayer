#include "fanmade.h"
#include "sha256.h"
#include <algorithm>
#include <climits>
#include <set>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#if defined(FANMADE_NETWORK)
#include <cpr/cpr.h>
#if defined(__ANDROID__)
#include <SDL3/SDL_iostream.h>
#endif
#endif
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__) || (defined(__unix__) && !defined(__ANDROID__))
#include <iconv.h>
#endif

namespace fanmade {
namespace {
const std::array<std::string,5> courses{"Easy","Normal","Hard","Oni","Edit"};
std::string trim(std::string s) {
    auto begin=s.find_first_not_of(" \t\r\n"), end=s.find_last_not_of(" \t\r\n");
    return begin==std::string::npos ? "" : s.substr(begin,end-begin+1);
}
std::string upper(std::string s) { for(auto& c:s) if(c>='a'&&c<='z') c-=32; return s; }
std::string line_text(std::string s) { for(auto& c:s) if(c=='\n'||c=='\r'||c=='\0') c=' '; return s; }
std::string path_key(const fs::path& p) { return fs::absolute(p).lexically_normal().generic_string(); }
std::string read(const fs::path& p) {
    std::ifstream in(p,std::ios::binary); if(!in) throw std::runtime_error("CACHE_READ_FAILED");
    return std::string(std::istreambuf_iterator<char>(in),{});
}
void write(const fs::path& p,const std::string& bytes) {
    fs::create_directories(p.parent_path());
    fs::path temp=p; temp += ".part";
    { std::ofstream out(temp,std::ios::binary|std::ios::trunc); out.write(bytes.data(),bytes.size()); out.flush(); if(!out) throw std::runtime_error("CACHE_WRITE_FAILED"); }
#ifdef _WIN32
    // MoveFileEx replaces the destination atomically on Windows as well.
    if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("CACHE_RENAME_FAILED");
#else
    fs::rename(temp,p);
#endif
}
bool hex_id(const std::string& s,size_t length) { return s.size()==length && s.find_first_not_of("0123456789abcdef")==std::string::npos; }
std::string str(const rapidjson::Value& v,const char* key) {
    if(!v.IsObject()||!v.HasMember(key)||!v[key].IsString()) throw std::runtime_error("API_STRING_INVALID");
    return {v[key].GetString(),v[key].GetStringLength()};
}
int64_t number(const rapidjson::Value& v,const char* key) {
    if(!v.IsObject()||!v.HasMember(key)||!v[key].IsInt64()||v[key].GetInt64()<0) throw std::runtime_error("API_NUMBER_INVALID");
    return v[key].GetInt64();
}
rapidjson::Document json(const std::string& text) {
    rapidjson::Document d; d.Parse(text.data(),text.size());
    if(d.HasParseError()||!d.IsObject()) throw std::runtime_error("API_JSON_INVALID"); return d;
}
std::string encode(const rapidjson::Value& d) { rapidjson::StringBuffer b; rapidjson::Writer<rapidjson::StringBuffer> w(b); d.Accept(w); return b.GetString(); }
void put(rapidjson::Document& d,const char* k,const std::string& v) { d.AddMember(rapidjson::Value(k,d.GetAllocator()),rapidjson::Value(v.c_str(),v.size(),d.GetAllocator()),d.GetAllocator()); }
void put(rapidjson::Document& d,const char* k,int64_t v) { d.AddMember(rapidjson::Value(k,d.GetAllocator()),rapidjson::Value(v),d.GetAllocator()); }
Score score_from(const rapidjson::Value& v) {
    Score s; s.id=str(v,"id"); s.song=str(v,"songId"); s.version=str(v,"versionId"); s.difficulty=str(v,"difficulty");
    s.good=number(v,"good"); s.ok=number(v,"ok"); s.bad=number(v,"bad"); s.score=number(v,"score"); s.drumroll=number(v,"drumroll");
    s.max_combo=number(v,"max_combo");
    return s;
}
Chart chart_from(const rapidjson::Value& v,const std::string& server) {
    Chart c; c.server=server; c.id=str(v,"id"); c.version=str(v,"versionId"); c.title=str(v,"title"); c.subtitle=str(v,"subtitle"); c.maker=str(v,"maker");
    c.tja_hash=str(v,"tjaHash"); c.audio_hash=str(v,"audioHash"); c.encoding=str(v,"encoding"); c.audio_name=str(v,"audioName");
    if(!hex_id(c.id,32)||!hex_id(c.version,32)||!hex_id(c.tja_hash,64)||!hex_id(c.audio_hash,64)) throw std::runtime_error("API_ID_INVALID");
    c.titles["en"]=c.title; c.subtitles["en"]=c.subtitle;
    for(auto pair:{std::make_pair("titleTranslations",&c.titles),std::make_pair("subtitleTranslations",&c.subtitles)}) {
        if(!v.HasMember(pair.first)||!v[pair.first].IsObject()) throw std::runtime_error("API_TRANSLATIONS_INVALID");
        // Windows headers define GetObject as a Win32 API macro.
        const auto& translations=v[pair.first];
        for(auto m=translations.MemberBegin();m!=translations.MemberEnd();++m)
            if(m->value.IsString()) (*pair.second)[m->name.GetString()]=m->value.GetString();
    }
    if(!v.HasMember("bpm")||!v["bpm"].IsNumber()||!v.HasMember("demoStart")||!v["demoStart"].IsNumber()) throw std::runtime_error("API_METADATA_INVALID");
    c.bpm=v["bpm"].GetDouble(); c.demo_start=v["demoStart"].GetDouble();
    if(!v.HasMember("difficulties")||!v["difficulties"].IsArray()) throw std::runtime_error("API_DIFFICULTIES_INVALID");
    for(auto& d:v["difficulties"].GetArray()) {
        auto name=str(d,"course"); auto it=std::find(courses.begin(),courses.end(),name); if(it==courses.end()) continue;
        auto& slot=c.difficulties[it-courses.begin()];
        if(!d.HasMember("cloudScoreEligible")||!d["cloudScoreEligible"].IsBool()) throw std::runtime_error("API_DIFFICULTY_INVALID");
        auto level=number(d,"level"), block=number(d,"blockIndex");
        if(level>100||block>100000) throw std::runtime_error("API_DIFFICULTY_INVALID");
        bool cloud=d["cloudScoreEligible"].GetBool();
        Difficulty diff{name,(int)level,(int)block,cloud,str(d,"player")};
        c.blocks.push_back(diff);
        if(!slot || (!slot->cloud&&cloud)) slot=diff;
    }
    return c;
}
std::string cached_audio_name(const Chart& c) {
    const auto extension=upper(fs::path(c.audio_name).extension().string());
    if(extension==".MP3") return "audio.mp3";
    if(extension==".OGG") return "audio.ogg";
    throw std::runtime_error("API_AUDIO_FORMAT_UNSUPPORTED");
}
std::string title_headers(const Chart& c) {
    std::string out = "MAKER:"+line_text(c.maker)+"\n";
    for(auto pair:{std::make_pair("TITLE",&c.titles),std::make_pair("SUBTITLE",&c.subtitles)})
        for(auto& [lang,value]:*pair.second) {
            if(lang!="en"&&lang!="ja"&&lang!="zh"&&lang!="ko") continue;
            out+=std::string(pair.first)+(lang=="en"?"":upper(lang))+":"+line_text(value)+"\n";
        }
    return out;
}
std::string to_utf8(const std::string& bytes,const std::string& encoding) {
    if(encoding=="utf-8") return bytes;
    if(encoding!="shift-jis") throw std::runtime_error("TJA_ENCODING_UNSUPPORTED");
#ifdef _WIN32
    int n=MultiByteToWideChar(932,0,bytes.data(),(int)bytes.size(),nullptr,0);
    if(n<=0) throw std::runtime_error("TJA_ENCODING_INVALID");
    std::wstring wide(n,L'\0'); MultiByteToWideChar(932,0,bytes.data(),(int)bytes.size(),wide.data(),n);
    n=WideCharToMultiByte(CP_UTF8,0,wide.data(),(int)wide.size(),nullptr,0,nullptr,nullptr);
    std::string out(n,'\0'); WideCharToMultiByte(CP_UTF8,0,wide.data(),(int)wide.size(),out.data(),n,nullptr,nullptr); return out;
#elif defined(__APPLE__) || (defined(__unix__) && !defined(__ANDROID__))
    iconv_t converter=iconv_open("UTF-8","CP932");
    if(converter==(iconv_t)-1) throw std::runtime_error("TJA_ENCODING_UNSUPPORTED");
    std::string out(bytes.size()*4+16,'\0'); size_t in_left=bytes.size(),out_left=out.size();
    char* in=const_cast<char*>(bytes.data()); char* dest=out.data();
    auto result=iconv(converter,&in,&in_left,&dest,&out_left); iconv_close(converter);
    if(result==(size_t)-1||in_left) throw std::runtime_error("TJA_ENCODING_INVALID"); out.resize(out.size()-out_left); return out;
#else
    throw std::runtime_error("SHIFT_JIS_UNSUPPORTED_ON_THIS_PLATFORM");
#endif
}
struct HttpError:std::runtime_error { int status; explicit HttpError(int s):runtime_error("HTTP_"+std::to_string(s)),status(s){} };
struct Endpoint {
    ServerConfig config;
    std::string id,token;
    std::mutex http_mutex;
    bool connected=false;
    std::atomic<bool> authenticated{false};
    int chart_count=-1;
    std::vector<Score> scores;
    std::map<std::tuple<std::string,std::string,std::string>,Score> best_scores;
    void index_score(const Score& score) {
        auto key=std::make_tuple(score.song,score.version,score.difficulty);
        auto it=best_scores.find(key);
        if(it==best_scores.end()||score.score>it->second.score) best_scores[key]=score;
    }
};
using TransferCallback = std::function<void(uint64_t, uint64_t)>;
#if defined(FANMADE_NETWORK)
std::string request(Endpoint& e,const std::string& path,const std::string& body="",const std::string& key="",size_t limit=64*1024*1024, std::shared_ptr<std::atomic_bool> cancel={}, TransferCallback progress={}) {
    if(cancel && *cancel) throw std::runtime_error("DOWNLOAD_CANCELLED");
    cpr::Session s;
#if defined(__ANDROID__)
    // Android's OpenSSL curl cannot use the Java system trust store. SDL reads
    // the CA bundle from APK assets, even after the game changes directory.
    static const std::string ca_bundle=[] {
        size_t size=0;
        void* data=SDL_LoadFile("cacert.pem", &size);
        if(!data) throw std::runtime_error("TLS_CA_BUNDLE_MISSING");
        std::unique_ptr<void, decltype(&SDL_free)> owned(data, SDL_free);
        if(!size) throw std::runtime_error("TLS_CA_BUNDLE_INVALID");
        return std::string(static_cast<const char*>(data), size);
    }();
    curl_blob ca{const_cast<char*>(ca_bundle.data()), ca_bundle.size(), CURL_BLOB_COPY};
    if(curl_easy_setopt(s.GetCurlHolder()->handle, CURLOPT_CAINFO_BLOB, &ca)!=CURLE_OK)
        throw std::runtime_error("TLS_CA_BUNDLE_INVALID");
#endif
    if(cancel) s.SetCancellationParam(cancel);
    if(progress) s.SetProgressCallback(cpr::ProgressCallback{[&](cpr::cpr_pf_arg_t total, cpr::cpr_pf_arg_t now, cpr::cpr_pf_arg_t, cpr::cpr_pf_arg_t, intptr_t) {
        progress(now > 0 ? static_cast<uint64_t>(now) : 0,
                 total > 0 ? static_cast<uint64_t>(total) : 0);
        return !cancel || !*cancel;
    }});
    s.SetUrl(cpr::Url{e.config.base_url+path});
    s.SetTimeout(cpr::Timeout{path.find("/versions/")==std::string::npos?15000:120000});
    s.SetConnectTimeout(cpr::ConnectTimeout{5000});
    s.SetRedirect(cpr::Redirect{false});
    // Explicit empty proxies also disable libcurl's environment proxy fallback.
    s.SetProxies(cpr::Proxies{{"http",e.config.http_proxy},{"https",e.config.http_proxy}});
    if(!e.config.http_proxy.empty()) curl_easy_setopt(s.GetCurlHolder()->handle, CURLOPT_NOPROXY, "");
    cpr::Header headers{{"Accept","application/json"}};
    if(!e.token.empty()) headers["Authorization"]="Bearer "+e.token;
    if(!body.empty()) { headers["Content-Type"]="application/json"; s.SetBody(cpr::Body{body}); }
    if(!key.empty()) headers["Idempotency-Key"]=key;
    s.SetHeader(headers);
    std::string bytes;
    bool size_exceeded=false;
    s.SetWriteCallback(cpr::WriteCallback{[&](std::string_view part,intptr_t) {
        if(part.size()>limit-bytes.size()) { size_exceeded=true; return false; }
        bytes.append(part); return true;
    }});
    auto r=body.empty()?s.Get():s.Post();
    if(cancel && *cancel) throw std::runtime_error("DOWNLOAD_CANCELLED");
    if(size_exceeded) throw std::runtime_error("RESPONSE_SIZE_LIMIT_EXCEEDED");
    switch(r.error.code) {
        case cpr::ErrorCode::OK: break;
        case cpr::ErrorCode::COULDNT_RESOLVE_HOST: throw std::runtime_error("NETWORK_DNS_ERROR");
        case cpr::ErrorCode::COULDNT_RESOLVE_PROXY: throw std::runtime_error("NETWORK_PROXY_DNS_ERROR");
        case cpr::ErrorCode::COULDNT_CONNECT: throw std::runtime_error("NETWORK_CONNECT_ERROR");
        case cpr::ErrorCode::OPERATION_TIMEDOUT: throw std::runtime_error("NETWORK_TIMEOUT");
        case cpr::ErrorCode::PEER_FAILED_VERIFICATION: throw std::runtime_error("TLS_CERTIFICATE_VERIFY_FAILED");
        case cpr::ErrorCode::SSL_CACERT_BADFILE: throw std::runtime_error("TLS_CA_BUNDLE_INVALID");
        case cpr::ErrorCode::SSL_CONNECT_ERROR: throw std::runtime_error("TLS_HANDSHAKE_FAILED");
        default: throw std::runtime_error("NETWORK_ERROR_"+std::to_string(static_cast<int>(r.error.code)));
    }
    if(r.status_code<200||r.status_code>=300) throw HttpError((int)r.status_code);
    return bytes;
}
#else
std::string request(Endpoint&,const std::string&,const std::string& ="",const std::string& ="",size_t =64*1024*1024, std::shared_ptr<std::atomic_bool> ={}, TransferCallback ={}) { throw std::runtime_error("FANMADE_NETWORK_DISABLED"); }
#endif
void login(Endpoint& e) {
    e.token.clear();
    if(e.config.username.empty()||e.config.password.empty()) { e.authenticated=false; return; }
    rapidjson::Document d; d.SetObject(); put(d,"username",e.config.username); put(d,"password",e.config.password);
    try {
        auto reply=json(request(e,"/api/v1/game/login",encode(d))); auto token=str(reply,"accessToken");
        if(!hex_id(token,64)) throw std::runtime_error("API_TOKEN_INVALID");
        e.token=std::move(token);
        e.authenticated=true;
    } catch(const HttpError& err) {
        // A temporary refresh failure keeps the existing account's queue
        // eligible for retry. Rejected credentials instead enter guest mode.
        if(err.status==401||err.status==403) e.authenticated=false;
        throw;
    }
}
std::string authorized(Endpoint& e,const std::string& path,const std::string& body="",const std::string& key="", std::shared_ptr<std::atomic_bool> cancel={}) {
    try { return request(e,path,body,key,64*1024*1024,cancel); }
    catch(const HttpError& err) { if(err.status!=401||!e.authenticated) throw; login(e); return request(e,path,body,key,64*1024*1024,cancel); }
}
std::string random_key() {
    std::random_device r; std::string bytes; for(int i=0;i<8;i++) bytes+=std::to_string(r()); return sha256(bytes);
}
}

std::string sha256(const std::string& bytes) { return crypto::to_hex(crypto::sha256(bytes)); }

std::string playable_tja(const std::string& utf8,const Chart& chart) {
    std::istringstream input(utf8.substr(utf8.compare(0,3,"\xef\xbb\xbf")==0?3:0));
    std::vector<std::string> globals,headers; std::string line,body,output=title_headers(chart)+"WAVE:"+cached_audio_name(chart)+"\n";
    bool in_block=false,seen_course=false; int block=-1; std::map<int,Difficulty> wanted; std::map<int,bool> found;
    for(auto& d:chart.difficulties) if(d) {
        wanted[d->block_index]=*d;
        if(!d->cloud) for(auto& other:chart.blocks)
            if(other.course==d->course) wanted[other.block_index]=other;
    }
    auto header=[](const std::string& line) {
        auto key=upper(trim(line.substr(0,line.find(':'))));
        return key.rfind("TITLE",0)!=0&&key.rfind("SUBTITLE",0)!=0&&key!="MAKER"&&key!="WAVE"&&key!="BGMOVIE"&&key!="PREIMAGE"&&key!="COURSE"&&key!="LEVEL"&&key!="STYLE";
    };
    while(std::getline(input,line)) {
        line=trim(line.substr(0,line.find("//"))); if(line.empty()) continue;
        auto key=upper(trim(line.substr(0,line.find(':'))));
        if(line.rfind("#START",0)==0) { in_block=true; block++; body.clear(); continue; }
        if(line=="#END") {
            if(in_block&&wanted.count(block)) {
                auto d=wanted.at(block);
                output+="COURSE:"+d.course+"\nLEVEL:"+std::to_string(d.level)+"\nSTYLE:"+(d.cloud?"Single":"Double")+"\n";
                for(auto& h:globals) if(header(h)) output+=h+"\n";
                for(auto& h:headers) if(header(h)) output+=h+"\n";
                output+="#START"+(d.player.empty()?std::string{}:" "+d.player)+"\n"+body+"#END\n"; found[block]=true;
            }
            in_block=false; continue;
        }
        if(in_block) { body+=line+"\n"; continue; }
        if(key=="COURSE") { seen_course=true; headers.clear(); continue; }
        (seen_course?headers:globals).push_back(line);
    }
    if(found.size()!=wanted.size()||in_block) throw std::runtime_error("TJA_BLOCK_MISMATCH");
    return output;
}

struct Client::Impl {
    mutable std::mutex mutex;
    std::mutex prepare_mutex;
    std::mutex catalog_mutex;
    struct CategoryFolder { std::string server, id; int count = -1; };
    std::map<std::string,CategoryFolder> categories;
    std::mutex pump_mutex;
    std::atomic<bool> bootstrapping{false};
    std::atomic<uint64_t> revision{0};
    fs::path cache,root;
    std::map<std::string,std::shared_ptr<Endpoint>> endpoints;
    std::map<std::string,Chart> charts;
    std::string message;
    std::future<void> uploads;
    std::chrono::steady_clock::time_point retry{};
    void status(const std::string& s) { std::lock_guard lock(mutex); message=s; }
    void drain() {
        for(auto& [id,e]:endpoints) {
            if(!e->connected||!e->authenticated) continue;
            auto folder=cache/"pending"/id; if(!fs::exists(folder)) continue;
            for(auto& file:fs::directory_iterator(folder)) {
                if(file.path().extension()!=".json") continue;
                try {
                    auto body=read(file.path());
                    std::lock_guard transport(e->http_mutex);
                    auto result=json(authorized(*e,"/api/v1/game/scores",body,file.path().stem().string()));
                    auto score=score_from(result);
                    { std::lock_guard lock(mutex);
                      if(std::none_of(e->scores.begin(),e->scores.end(),[&](auto& s){return s.id==score.id;})) e->scores.push_back(score);
                      e->index_score(score); revision++;
                      message=e->config.name+": score uploaded";
                    }
                    fs::remove(file.path());
                } catch(const HttpError& err) {
                    status(e->config.name+": score pending ("+err.what()+")");
                    if(err.status>=400&&err.status<500&&err.status!=401&&err.status!=408&&err.status!=429) {
                        auto rejected=file.path(); rejected.replace_extension(".rejected"); fs::rename(file.path(),rejected);
                        status(e->config.name+": score rejected ("+err.what()+"), saved locally");
                    } else break;
                } catch(const std::exception& err) { status(e->config.name+": score pending ("+err.what()+")"); break; }
            }
        }
    }
};
Client::Client():impl(std::make_unique<Impl>()){}
Client::~Client() { if(impl->uploads.valid()) impl->uploads.wait(); }
Client& client(){ static Client instance; return instance; }

void Client::bootstrap(const std::vector<ServerConfig>& servers,const fs::path& cache) {
    std::lock_guard catalog(impl->catalog_mutex);
    std::unique_lock pump(impl->pump_mutex);
    impl->bootstrapping = true;
    struct Reset { std::atomic<bool>& flag; ~Reset(){flag=false;} } reset{impl->bootstrapping};
    auto pending=std::move(impl->uploads);
    pump.unlock();
    if(pending.valid()) pending.get();
    impl->cache=fs::absolute(cache); impl->root=impl->cache/"catalog";
    fs::create_directories(impl->root);
    // Catalog is disposable display metadata. Content-addressed objects and
    // pending/rejected score submissions live outside it and survive refresh.
    fs::remove_all(impl->root); fs::create_directories(impl->root);
    { std::lock_guard lock(impl->mutex); impl->charts.clear(); impl->categories.clear(); impl->endpoints.clear(); }
    for(auto config:servers) {
        auto e=std::make_shared<Endpoint>(); e->config=std::move(config);
        while(!e->config.base_url.empty()&&e->config.base_url.back()=='/') e->config.base_url.pop_back();
        e->id=sha256(e->config.base_url+"\n"+e->config.username);
        if(impl->endpoints.count(e->id)) continue;
        { std::lock_guard lock(impl->mutex); impl->endpoints[e->id]=e; }
        auto dir=impl->root/e->id;
        try {
            auto& url=e->config.base_url;
            if((url.rfind("http://",0)!=0&&url.rfind("https://",0)!=0)||url.find_first_of("?#@ \r\n")!=std::string::npos) throw std::runtime_error("SERVER_URL_INVALID");
            impl->status(e->config.name+": loading catalog");
            std::lock_guard transport(e->http_mutex);
            std::string login_error;
            try { login(*e); }
            catch(const std::exception& err) { login_error=err.what(); }
            auto snapshot=json(authorized(*e,"/api/v1/game/bootstrap"));
            if(!snapshot.HasMember("categories")||!snapshot["categories"].IsArray()||!snapshot.HasMember("scores")||!snapshot["scores"].IsArray()) throw std::runtime_error("API_BOOTSTRAP_INVALID");
            struct Category { std::string id,title,genre; int count=-1; };
            std::vector<Category> categories;
            for(auto& v:snapshot["categories"].GetArray()) {
                Category c{str(v,"id"),str(v,"title"),str(v,"genre")};
                if(c.id.empty()||c.id.size()>64||c.id[0]<'a'||c.id[0]>'z'||c.id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-")!=std::string::npos)
                    throw std::runtime_error("API_CATEGORY_INVALID");
                if(std::any_of(categories.begin(),categories.end(),[&](const auto& other){return other.id==c.id;})) throw std::runtime_error("API_CATEGORY_INVALID");
                if(v.HasMember("chartCount")) {
                    auto n=number(v,"chartCount");
                    if(n>INT_MAX) throw std::runtime_error("API_COUNT_INVALID");
                    c.count=static_cast<int>(n);
                }
                categories.push_back(std::move(c));
            }
            if(snapshot.HasMember("chartCount")) {
                auto n=number(snapshot,"chartCount");
                if(n>INT_MAX) throw std::runtime_error("API_COUNT_INVALID");
                e->chart_count=static_cast<int>(n);
            }
            std::vector<Score> scores;
            if(e->authenticated) for(auto& v:snapshot["scores"].GetArray()) scores.push_back(score_from(v));
            write(dir/"box.def","#TITLE:"+line_text(e->config.name)+"\n#GENRE:Namco Original\n");
            for(auto& c:categories) {
                auto path=dir/c.id;
                write(path/"box.def","#TITLE:"+line_text(c.title)+"\n#GENRE:"+line_text(c.genre)+"\n");
                std::lock_guard lock(impl->mutex);
                impl->categories[path_key(path)]={e->id,c.id,c.count};
            }
            { std::lock_guard lock(impl->mutex); e->scores=std::move(scores); for(auto& score:e->scores) e->index_score(score); e->connected=true; }
            auto status=e->config.name+": "+std::to_string(categories.size())+" categories ready";
            if(!e->authenticated) status+=" (guest; scores disabled"+(login_error.empty()?std::string():"; login failed: "+login_error)+")";
            impl->status(status);
        } catch(const std::exception& err) {
            impl->status(e->config.name+": "+err.what());
            write(dir/"box.def","#TITLE:"+line_text(e->config.name)+" ["+err.what()+"]\n");
        }
    }
    impl->revision++;
    impl->bootstrapping = false;
    update();
}
std::vector<fs::path> Client::song_paths(std::vector<fs::path> local) const {
    if(!impl->endpoints.empty()) local.push_back(impl->root); return local;
}
bool Client::is_category(const fs::path& path) const {
    std::lock_guard lock(impl->mutex);
    return impl->categories.count(path_key(path))!=0;
}
bool Client::is_server(const fs::path& path) const {
    std::lock_guard lock(impl->mutex);
    return !impl->root.empty() && path_key(path.parent_path())==path_key(impl->root) && impl->endpoints.count(path.filename().string());
}
std::optional<int> Client::folder_count(const fs::path& path) const {
    std::lock_guard lock(impl->mutex);
    if(impl->root.empty()) return {};
    auto it=impl->categories.find(path_key(path));
    if(it!=impl->categories.end()) return it->second.count;
    auto server=impl->endpoints.find(path.filename().string());
    if(path_key(path.parent_path())==path_key(impl->root)&&server!=impl->endpoints.end()) return server->second->chart_count;
    return {};
}
bool Client::load_directory(const fs::path& path) {
    std::lock_guard loading(impl->catalog_mutex);
    std::shared_ptr<Endpoint> e;
    std::vector<Impl::CategoryFolder> folders;
    {
        std::lock_guard lock(impl->mutex);
        if(impl->root.empty() || path_key(path.parent_path())!=path_key(impl->root)) return false;
        auto it=impl->endpoints.find(path.filename().string());
        if(it==impl->endpoints.end()) return false;
        e=it->second;
        for(const auto& [key,folder]:impl->categories) if(folder.server==e->id) folders.push_back(folder);
    }
    // Publish only a complete snapshot. Failed refreshes retain the previous
    // on-disk catalog and counts, but the navigator keeps the categories hidden.
    auto staging=impl->cache/"catalog-staging"/e->id;
    auto previous=impl->cache/"catalog-previous"/e->id;
    try {
        if(!e->connected) throw std::runtime_error("SERVER_NOT_CONNECTED");
        fs::remove_all(staging);
        fs::create_directories(staging);
        write(staging/"box.def",read(path/"box.def"));
        std::map<std::string,Chart> charts;
        std::set<std::string> unique;
        for(auto& folder:folders) {
            impl->status(e->config.name+": loading "+folder.id);
            rapidjson::Document result;
            {
                std::lock_guard transport(e->http_mutex);
                result=json(authorized(*e,"/api/v1/game/categories/"+folder.id+"/charts"));
            }
            if(str(result,"categoryId")!=folder.id||!result.HasMember("charts")||!result["charts"].IsArray()) throw std::runtime_error("API_CATEGORY_INVALID");
            write(staging/folder.id/"box.def",read(path/folder.id/"box.def"));
            folder.count=0;
            for(auto& value:result["charts"].GetArray()) {
                auto c=chart_from(value,e->id);
                auto chart_path=path/folder.id/(c.id+".tja");
                std::string preview="// Fanmade catalog metadata only. Never play this file.\n"+title_headers(c)+"BPM:"+std::to_string(c.bpm)+"\nDEMOSTART:"+std::to_string(c.demo_start)+"\nWAVE:unavailable.ogg\n";
                bool supported=false;
                for(auto& d:c.difficulties) if(d) { supported=true; preview+="COURSE:"+d->course+"\nLEVEL:"+std::to_string(d->level)+"\n#START\n0,\n#END\n"; }
                if(!supported) continue;
                if(!charts.emplace(path_key(chart_path),c).second) throw std::runtime_error("API_DUPLICATE_CHART");
                write(staging/folder.id/(c.id+".tja"),preview);
                unique.insert(c.id);
                folder.count++;
            }
        }
        fs::create_directories(previous.parent_path());
        fs::remove_all(previous);
        {
            std::lock_guard lock(impl->mutex);
            // Prepare allocations before swapping directories so exceptions
            // cannot leave the path registry pointing at another snapshot.
            auto next_charts=impl->charts;
            for(auto it=next_charts.begin();it!=next_charts.end();) {
                if(it->second.server==e->id && it->first.starts_with(path_key(path)+"/")) it=next_charts.erase(it);
                else ++it;
            }
            next_charts.insert(charts.begin(),charts.end());
            fs::rename(path,previous);
            try { fs::rename(staging,path); }
            catch(...) { fs::rename(previous,path); throw; }
            impl->charts.swap(next_charts);
            for(const auto& folder:folders) impl->categories.at(path_key(path/folder.id)).count=folder.count;
            e->chart_count=static_cast<int>(unique.size());
            impl->revision++;
        }
        std::error_code ignored;
        fs::remove_all(previous,ignored);
        impl->status(e->config.name+": "+std::to_string(unique.size())+" songs refreshed");
        return true;
    } catch(const std::exception& err) {
        std::error_code ignored;
        fs::remove_all(staging,ignored);
        impl->status(e->config.name+": refresh failed ("+err.what()+"); reopen server to retry");
        throw;
    }
}
std::optional<Chart> Client::chart(const fs::path& path) const {
    std::lock_guard lock(impl->mutex); auto it=impl->charts.find(path_key(path));
    return it==impl->charts.end()?std::nullopt:std::optional<Chart>(it->second);
}
std::optional<Score> Client::best(const fs::path& path,int difficulty) const {
    auto c=chart(path); if(!c||difficulty<0||difficulty>=5||!c->difficulties[difficulty]||!c->difficulties[difficulty]->cloud) return {};
    std::lock_guard lock(impl->mutex);
    const auto& best=impl->endpoints.at(c->server)->best_scores;
    auto it=best.find(std::make_tuple(c->id,c->version,courses[difficulty]));
    return it==best.end()?std::nullopt:std::optional<Score>(it->second);
}
fs::path Client::prepare(const fs::path& path, std::shared_ptr<std::atomic_bool> cancel, DownloadCallback progress) {
    std::lock_guard preparing(impl->prepare_mutex);
    DownloadProgress snapshot;
    auto publish = [&] { if(progress) progress(snapshot); };
    publish();
    auto selected=chart(path); if(!selected) return path; auto c=*selected;
    auto e=impl->endpoints.at(c.server);
    std::lock_guard transport(e->http_mutex);
    impl->status(e->config.name+": checking file hashes");
    auto current=json(authorized(*e,"/api/v1/charts/"+c.id,"","",cancel));
    c=chart_from(current,c.server); // Revisions and renamed metadata are refreshed at loading time.
    auto dir=impl->cache/"objects"/c.server/c.id/c.version;
    auto base="/api/v1/charts/"+c.id+"/versions/"+c.version+"/";
    snapshot.stage = DownloadProgress::Stage::Files;
    auto ensure=[&](const fs::path& file,const std::string& digest,const std::string& kind,size_t limit, FileProgress& transfer) {
        if(cancel && *cancel) throw std::runtime_error("DOWNLOAD_CANCELLED");
        if(fs::exists(file)) {
            transfer.state = FileProgress::State::Verifying;
            publish();
            auto cached = read(file);
            if(sha256(cached)==digest) {
                transfer = {FileProgress::State::Cached, cached.size(), cached.size()};
                publish();
                return;
            }
        }
        impl->status(e->config.name+": downloading "+kind);
        transfer = {FileProgress::State::Downloading, 0, 0};
        publish();
        auto bytes=request(*e,base+kind,"","",limit,cancel,[&](uint64_t received, uint64_t total) {
            if(transfer.received==received && transfer.total==total) return;
            transfer.received=received; transfer.total=total;
            publish();
        });
        transfer = {FileProgress::State::Verifying, bytes.size(), bytes.size()};
        publish();
        if(sha256(bytes)!=digest) throw std::runtime_error("DOWNLOAD_HASH_MISMATCH");
        write(file,bytes);
        transfer.state = FileProgress::State::Complete;
        publish();
    };
    ensure(dir/"original.tja",c.tja_hash,"tja",4*1024*1024,snapshot.chart);
    const auto audio_path=dir/cached_audio_name(c);
    const auto old_audio_path=dir/"audio.ogg";
    // Older clients stored MP3 bytes under .ogg. Reuse only a verified cache.
    if(audio_path!=old_audio_path && !fs::exists(audio_path) && fs::exists(old_audio_path)
       && sha256(read(old_audio_path))==c.audio_hash) {
        fs::rename(old_audio_path,audio_path);
    }
    ensure(audio_path,c.audio_hash,"audio",256*1024*1024,snapshot.audio);
    snapshot.stage = DownloadProgress::Stage::Preparing;
    publish();
    auto playable=dir/"play.tja";
    write(playable,playable_tja(to_utf8(read(dir/"original.tja"),c.encoding),c));
    { std::lock_guard lock(impl->mutex); impl->charts[path_key(playable)]=c; impl->charts[path_key(path)]=c; impl->revision++; }
    impl->status(e->config.name+": ready");
    snapshot.stage = DownloadProgress::Stage::Ready;
    publish();
    return playable;
}
void Client::submit(const fs::path& path,int difficulty,const Score& score) {
    auto c=chart(path); if(!c||difficulty<0||difficulty>=5||!c->difficulties[difficulty]||!c->difficulties[difficulty]->cloud) return;
    auto e=impl->endpoints.at(c->server);
    if(!e->connected||!e->authenticated) return;
    rapidjson::Document d; d.SetObject(); put(d,"songId",c->id); put(d,"versionId",c->version); put(d,"difficulty",courses[difficulty]);
    put(d,"good",score.good); put(d,"ok",score.ok); put(d,"bad",score.bad); put(d,"score",score.score); put(d,"drumroll",score.drumroll); put(d,"max_combo",score.max_combo);
    try { write(impl->cache/"pending"/c->server/(random_key()+".json"),encode(d)); impl->retry={}; update(); }
    catch(const std::exception& err) { impl->status(std::string("Score queue error: ")+err.what()); }
}
void Client::update() {
    std::lock_guard pump(impl->pump_mutex);
    if(impl->bootstrapping) return;
    if(impl->uploads.valid()) {
        if(impl->uploads.wait_for(std::chrono::seconds(0))!=std::future_status::ready) return;
        try { impl->uploads.get(); } catch(const std::exception& err) { impl->status(std::string("Score upload error: ")+err.what()); }
    }
    auto now=std::chrono::steady_clock::now();
    if(impl->endpoints.empty()||now<impl->retry) return;
    impl->retry=now+std::chrono::seconds(30);
    impl->uploads=std::async(std::launch::async,[this]{impl->drain();});
}
uint64_t Client::revision() const { return impl->revision; }
bool Client::online() const { std::lock_guard lock(impl->mutex); for(auto& [id,e]:impl->endpoints) if(e->connected) return true; return false; }
std::string Client::status() const { std::lock_guard lock(impl->mutex); return impl->message; }
} // namespace fanmade
