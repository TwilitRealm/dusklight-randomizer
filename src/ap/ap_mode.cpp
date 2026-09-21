#include "ap_mode.hpp"

#include "ap_client.hpp"
#include "text_safe.hpp"

#include "../../generator/randomizer.hpp"
#include "../../generator/utility/yaml.hpp"
#include "../item_ids.h"
#include "../paths.hpp"
#include "../randomizer_context.hpp"
#include "../session.hpp"
#include "../stages.h"
#include "../tools.h"
#include "../ui/rando_config.hpp"
#include "../verify_item_functions.h"

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_b_gnd.h"
#include "d/actor/d_a_demo_item.h"
#include "d/actor/d_a_obj_item.h"
#include "d/d_com_inf_game.h"
#include "d/d_file_select.h"
#include "d/d_stage.h"
#include "m_Do/m_Do_audio.h"

#include <mods/items.h>
#include <mods/svc/flow.hpp>
#include <mods/svc/hook.hpp>
#include <mods/svc/log.hpp>

#include <yaml-cpp/yaml.h>
#include <fmt/format.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

RandomizerContext WriteSeedData(randomizer::logic::world::World* world);

namespace ap {
namespace {

using randomizer::session::svc_mng;

constexpr const char* kGameModeId = "archipelago";
constexpr const char* kConnBlob = "ap_conn";
constexpr const char* kStateBlob = "ap_state";
constexpr const char* kSeedHashBlob = "seed_hash";  // written by the randomizer session
constexpr int kSlotDataVersion = 1;
constexpr uint8_t kApItem = dItemNo_Randomizer_NOENTRY_220_e;  // "Archipelago Item" (0xDC)
constexpr uint16_t kApItemDonorMessage = 120;  // Foolish Item get text; overridden while armed
constexpr int64_t kItemIdBase = 0x54500000;

// ---------------------------------------------------------------------------------------
// State

Client g_client;

void after_seed_activated();

struct Conn {
    std::string server = "archipelago.gg:38281";
    std::string slot;
    std::string password;
};

struct SaveState {
    int received = 0;     // server item index delivered into this save
    bool goal = false;
    std::string seed;     // AP seed name this save belongs to
    std::string slot;
};

enum class Phase {
    Idle,           // title / no AP save loaded
    NewSave,        // new-save window open, not yet generated
    Generating,     // seed generation thread running
    AwaitNewSave,   // generated; waiting for the host to create the save
    Playing,        // an AP save is loaded
};

Phase g_phase = Phase::Idle;
Conn g_conn;
SaveState g_state;
bool g_needsRegen = false;       // save loaded but its seed files are missing on this machine
std::string g_loadedHash;

// Slot data
bool g_haveSlot = false;
std::string g_slotSeed;
std::unordered_map<std::string, int64_t> g_locationIds;       // location name -> AP id
std::unordered_map<std::string, std::string> g_apItemText;    // location name -> get text
std::unordered_map<std::string, std::string> g_expected;      // location name -> item AP expects
std::unordered_map<std::string, std::vector<std::string>> g_checkToLocations;

struct ScanLoc {
    std::string name;
    int64_t id;
    YAML::Node meta;
};
std::vector<ScanLoc> g_scan;
size_t g_scanPos = 0;
std::unordered_set<int64_t> g_checked;  // known to the server (sent or reported back)
std::vector<int64_t> g_toSend;

// Items
std::vector<NetworkItem> g_serverItems;
int g_outstanding = -1;
std::string g_outstandingDesc;

// New-save UI
UiWindowHandle g_window = 0;
UiElementHandle g_statusText = 0;
std::string g_status = "Enter your Archipelago connection, then press Connect.";

// Generation thread
std::atomic<int> g_genStatus{0};  // 0 idle, 1 running, 2 ok, 3 failed
std::mutex g_genMutex;
std::string g_genHash;
std::string g_genError;

// AP item text
std::string g_armedText;
int g_armedFrames = 0;
std::string g_lastResolvedApLocation;
std::vector<mods::flow::MessageOverride> g_textOverrides;
ItemCheckHandle g_resolver = 0;
ItemGiveHandle g_observer = 0;

// Menu tab
UiMenuTabHandle g_menuTab = 0;
UiWindowHandle g_statusWindow = 0;
UiElementHandle g_statusWindowText = 0;

// Config
ConfigVarHandle g_cfgServer = 0;
ConfigVarHandle g_cfgSlot = 0;
ConfigVarHandle g_cfgModelScale = 0;
ConfigVarHandle g_cfgDebugLog = 0;

// ---------------------------------------------------------------------------------------
// Helpers

// Flushed debug trail next to the mod's data; the host log buffers too much to debug with.
void ap_log(const std::string& line) {
    bool enabled = false;
    if (g_cfgDebugLog != 0) {
        svc_mng.config->get_bool(svc_mng.mod_ctx, g_cfgDebugLog, &enabled);
    }
    if (!enabled) {
        return;
    }
    const char* dir = nullptr;
    if (svc_mng.host->data_dir(svc_mng.mod_ctx, &dir) != MOD_OK || dir == nullptr) {
        return;
    }
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    const std::string stamped = fmt::format("{:02}:{:02}:{:02} {}", tm.tm_hour, tm.tm_min,
        tm.tm_sec, line);
    if (std::FILE* f = std::fopen((std::filesystem::path(dir) / "ap_debug.log").string().c_str(), "a")) {
        std::fputs(stamped.c_str(), f);
        std::fputc('\n', f);
        std::fclose(f);
    }
}

std::unordered_map<int, std::string> g_itemNames;

const std::string& item_name(int id) {
    if (g_itemNames.empty()) {
        for (const auto& node : LOAD_EMBED_YAML(RANDO_DATA_PATH "items.yaml")) {
            g_itemNames[node["Id"].as<int>()] = node["Name"].as<std::string>();
        }
    }
    static const std::string unknown = "an item";
    const auto it = g_itemNames.find(id);
    return it != g_itemNames.end() ? it->second : unknown;
}

std::string rml_escape(std::string_view in) {
    std::string out;
    for (char c : in) {
        switch (c) {
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '&': out += "&amp;"; break;
        default: out += c;
        }
    }
    return out;
}

void toast(const std::string& title, const std::string& body, const char* type = nullptr,
    uint32_t ms = 0) {
    // Server chat and refusal messages land here, so bound what we are willing to render.
    const std::string t = rml_escape(message_safe(title, 80));
    const std::string b = rml_escape(message_safe(body, 400));
    UiToastDesc desc{sizeof(UiToastDesc)};
    desc.type = type;
    desc.title_rml = t.c_str();
    desc.body_rml = b.c_str();
    desc.duration_ms = ms;
    svc_mng.ui->push_toast(svc_mng.mod_ctx, &desc);
}

std::string config_string(ConfigVarHandle var) {
    if (var == 0) {
        return {};
    }
    size_t len = 0;
    if (svc_mng.config->get_string(svc_mng.mod_ctx, var, nullptr, 0, &len) != MOD_OK || len == 0) {
        return {};
    }
    std::string s(len, '\0');
    svc_mng.config->get_string(svc_mng.mod_ctx, var, s.data(), s.size() + 1, &len);
    s.resize(std::strlen(s.c_str()));
    return s;
}

template <typename T>
bool read_blob(const char* name, T& out) {
    size_t size = 0;
    if (svc_mng.save->get_blob(svc_mng.mod_ctx, name, nullptr, &size) != MOD_OK || size == 0) {
        return false;
    }
    std::string buf(size, '\0');
    if (svc_mng.save->get_blob(svc_mng.mod_ctx, name, buf.data(), &size) != MOD_OK) {
        return false;
    }
    json j = json::parse(buf, nullptr, false);
    if (j.is_discarded()) {
        return false;
    }
    out = T{};
    if constexpr (std::is_same_v<T, Conn>) {
        out.server = j.value("server", out.server);
        out.slot = j.value("slot", "");
        out.password = j.value("password", "");
    } else {
        out.received = j.value("received", 0);
        out.goal = j.value("goal", false);
        out.seed = j.value("seed", "");
        out.slot = j.value("slot", "");
    }
    return true;
}

void write_conn() {
    const std::string s =
        json{{"server", g_conn.server}, {"slot", g_conn.slot}, {"password", g_conn.password}}.dump();
    svc_mng.save->set_blob(svc_mng.mod_ctx, kConnBlob, s.data(), s.size());
}

void write_state() {
    const std::string s = json{{"received", g_state.received}, {"goal", g_state.goal},
        {"seed", g_state.seed}, {"slot", g_state.slot}}
                              .dump();
    svc_mng.save->set_blob(svc_mng.mod_ctx, kStateBlob, s.data(), s.size());
}

std::string status_line() {
    switch (g_client.state()) {
    case State::Connected:
        return "Connected to " + g_client.info().server + " as " + g_client.info().slot;
    case State::Connecting:
    case State::Handshaking:
        return "Connecting to " + g_client.info().server + "...";
    case State::Refused:
        return "Connection refused: " + g_client.lastError();
    default:
        return g_client.lastError().empty() ? "Disconnected" :
                                              "Disconnected: " + g_client.lastError();
    }
}

// ---------------------------------------------------------------------------------------
// Slot data -> lookup tables

void build_check_map() {
    g_checkToLocations.clear();
    g_scan.clear();
    g_scanPos = 0;
    const auto locations = LOAD_EMBED_YAML(RANDO_DATA_PATH "locations.yaml");
    auto add = [](const std::string& check, const std::string& loc) {
        g_checkToLocations[check].push_back(loc);
    };
    for (const auto& node : locations) {
        const std::string name = node["Name"].as<std::string>();
        const auto idIt = g_locationIds.find(name);
        if (idIt == g_locationIds.end()) {
            continue;  // not an AP location in this slot (vanilla-locked or removed)
        }
        const YAML::Node meta = node["Metadata"];
        g_scan.push_back({name, idIt->second, meta});
        if (!meta.IsMap()) {
            continue;
        }
        auto stage = [](const YAML::Node& n) -> std::string {
            const int s = n["Stage"].as<int>();
            return (s >= 0 && s < static_cast<int>(std::size(allStages))) ? allStages[s] : "";
        };
        for (const auto& c : meta["Chest"]) {
            add(fmt::format("chest:{}:{}", stage(c), c["Tbox Id"].as<int>()), name);
        }
        for (const auto& c : meta["Poe"]) {
            add(fmt::format("poe:{}:{}", stage(c), c["Flag"].as<int>()), name);
        }
        for (const auto& c : meta["Freestanding Item"]) {
            const int flag = c["Flag"].as<int>();
            add(fmt::format("freestanding:{}:{}", stage(c), flag), name);
            if (flag == 0x9F) {
                add(fmt::format("boss:{}", stage(c)), name);
            }
        }
        for (const auto& c : meta["Sky Character"]) {
            add(fmt::format("sky:{}:{}", stage(c), c["Room"].as<int>()), name);
        }
        for (const auto& c : meta["Golden Wolf"]) {
            add(fmt::format("golden_wolf:{}", c["Flag"].as<int>()), name);
        }
        for (const auto& c : meta["Bug Reward"]) {
            add(fmt::format("bug:{}", c["Item Id"].as<int>()), name);
        }
        for (const auto& c : meta["Shop"]) {
            add(fmt::format("shop:{}:{}:{}", stage(c), c["Room"].as<int>(), c["Item"].as<int>()), name);
        }
        for (const auto& c : meta["Name Lookup"]) {
            add(nameLookupOverride(c.as<std::string>()), name);
        }
    }
}

std::vector<std::string> locations_for_check(const char* check) {
    if (check == nullptr) {
        return {};
    }
    if (const auto it = g_checkToLocations.find(check); it != g_checkToLocations.end()) {
        return it->second;
    }
    // Ook drops the Gale Boomerang as a freestanding item in his own stage
    const std::string ook = fmt::format("freestanding:{}:", allStages[Ook]);
    if (std::strncmp(check, ook.c_str(), ook.size()) == 0) {
        return {"Forest Temple Gale Boomerang"};
    }
    return {};
}

bool load_slot_data(const json& slotData, std::string& err) {
    if (slotData.value("version", 0) != kSlotDataVersion) {
        err = "This slot was generated with an incompatible apworld version.";
        return false;
    }
    g_locationIds.clear();
    g_apItemText.clear();
    for (const auto& [name, id] : slotData.value("location_ids", json::object()).items()) {
        g_locationIds[name] = id.get<int64_t>();
    }
    g_expected.clear();
    for (const auto& [loc, v] : slotData.value("placements", json::object()).items()) {
        g_expected[loc] = v.is_object() ? fmt::format("{} ({})", v.value("name", "?"),
                                              v.value("player", "?"))
                                        : v.get<std::string>();
        if (v.is_object()) {
            std::string who = message_safe(v.value("player", "someone"), 40);
            std::string what = message_safe(v.value("name", "item"), 60);
            if (who.empty()) {
                who = "someone";
            }
            if (what.empty()) {
                what = "an item";
            }
            g_apItemText[loc] = fmt::format("You found {}'s\n{}!", who, what);
        }
    }
    g_slotSeed = slotData.value("seed", "");
    build_check_map();
    g_haveSlot = true;
    return true;
}

// ---------------------------------------------------------------------------------------
// Seed generation from slot data (runs on a worker thread)

std::string sanitize(std::string s) {
    for (auto& c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
            c = '_';
        }
    }
    return s;
}

bool generate_seed(const json& slotData, const std::string& slot, std::string& outHash,
    std::string& outError) {
    namespace fs = std::filesystem;
    try {
        const std::string seedName = slotData.value("seed", "");
        const fs::path base =
            randomizer::paths::GetRandomizerPath() / "archipelago" / sanitize(seedName + "-" + slot);
        fs::create_directories(base);

        YAML::Node settings;
        settings["Seed"] = "AP-" + seedName + "-" + slot;
        settings["Plandomizer"] = true;
        settings["Generate Spoiler Log"] = false;
        settings["Starting Inventory"] = YAML::Node(YAML::NodeType::Map);
        settings["Excluded Locations"] = YAML::Node(YAML::NodeType::Sequence);
        settings["Mixed Entrance Pools"] = YAML::Node(YAML::NodeType::Sequence);
        for (const auto& [k, v] : slotData.value("settings", json::object()).items()) {
            settings[k] = v.get<std::string>();
        }
        std::ofstream(base / "settings.yaml") << YAML::Dump(settings);

        YAML::Node plando;
        YAML::Node locs(YAML::NodeType::Map);
        for (const auto& [loc, v] : slotData.value("placements", json::object()).items()) {
            locs[loc] = v.is_string() ? v.get<std::string>() : v.value("item", "Archipelago Item");
        }
        plando["World 1"]["Locations"] = locs;
        const fs::path plandoPath = base / "plando.yaml";
        std::ofstream(plandoPath) << YAML::Dump(plando);

        YAML::Node prefs;
        prefs["Plandomizer Path"] = plandoPath.generic_string();
        std::ofstream(base / "preferences.yaml") << YAML::Dump(prefs);

        randomizer::g_archipelagoMode = true;
        randomizer::Randomizer rando{base};
        auto err = rando.Generate();
        randomizer::g_archipelagoMode = false;
        if (err.has_value()) {
            outError = *err;
            return false;
        }
        RandomizerContext ctx = WriteSeedData(rando.GetWorld());
        ctx.mHash = rando.GetConfig().GetHash();
        fs::create_directories(ctx.GetSeedDataPath().parent_path());
        if (auto werr = ctx.WriteToFile(); werr.has_value()) {
            outError = *werr;
            return false;
        }
        outHash = ctx.mHash;
        return true;
    } catch (const std::exception& e) {
        randomizer::g_archipelagoMode = false;
        outError = e.what();
        return false;
    }
}

void start_generation(const json& slotData, const std::string& slot) {
    g_genStatus = 1;
    std::thread([slotData, slot] {
        std::string hash, error;
        const bool ok = generate_seed(slotData, slot, hash, error);
        {
            std::lock_guard lock{g_genMutex};
            g_genHash = hash;
            g_genError = error;
        }
        g_genStatus = ok ? 2 : 3;
    }).detach();
}

bool seed_files_exist(const std::string& hash) {
    std::error_code ec;
    return !hash.empty() &&
           std::filesystem::exists(randomizer::paths::GetRandomizerSeedsPath() / hash / "seed.dat", ec);
}

// ---------------------------------------------------------------------------------------
// Client callbacks

json g_lastSlotData;

void on_connected(const json& p) {
    const json slotData = p.value("slot_data", json::object());
    std::string err;
    if (!load_slot_data(slotData, err)) {
        g_status = err;
        toast("Archipelago", err, "warning");
        g_client.disconnect();
        return;
    }
    g_lastSlotData = slotData;
    g_checked.clear();
    for (const auto& id : p.value("checked_locations", json::array())) {
        g_checked.insert(id.get<int64_t>());
    }

    ap_log(fmt::format("connected in phase {}: {} locations, {} ap placements",
        static_cast<int>(g_phase), g_locationIds.size(), g_apItemText.size()));
    if (g_phase == Phase::NewSave) {
        g_status = "Connected. Building your seed...";
        g_phase = Phase::Generating;
        start_generation(slotData, g_client.info().slot);
        return;
    }
    if (g_phase == Phase::Playing) {
        if (!g_state.seed.empty() && g_state.seed != g_slotSeed) {
            toast("Archipelago", "This save belongs to a different multiworld seed. Disconnected.",
                "warning", 8000);
            g_client.disconnect();
            g_haveSlot = false;
            return;
        }
        if (g_needsRegen) {
            toast("Archipelago", "Rebuilding this save's seed from the server...");
            g_phase = Phase::Generating;
            start_generation(slotData, g_client.info().slot);
            return;
        }
        g_client.sendSync();  // also proves the transport can send after the handshake
        toast("Archipelago", status_line(), "success", 3000);
        if (g_state.goal) {
            g_client.sendGoal();
        }
    }
}

void on_items(int index, const std::vector<NetworkItem>& items) {
    if (index == 0) {
        g_serverItems.clear();
    }
    if (static_cast<size_t>(index) != g_serverItems.size()) {
        g_client.sendSync();  // gap: ask for the full list again
        return;
    }
    g_serverItems.insert(g_serverItems.end(), items.begin(), items.end());
}

void on_print(const std::string& text, const json& msg) {
    const std::string type = msg.value("type", "");
    const int me = g_client.slot();
    if (type == "ItemSend" || type == "ItemCheat") {
        const int receiver = msg.value("receiving", -1);
        const int sender = msg.contains("item") ? msg["item"].value("player", -1) : -1;
        if (receiver == me || sender == me) {
            if (receiver == me && sender == me) {
                return;  // our own item at our own location: the game already showed it
            }
            toast("Archipelago", text, nullptr, 4000);
        }
    } else if (type == "Goal" || type == "Release" || type == "Collect" || type == "Countdown") {
        toast("Archipelago", text, nullptr, 4000);
    } else if (type == "Chat" || type == "ServerChat") {
        toast("Archipelago", text, nullptr, 5000);
    }
}

void on_disconnected(const std::string& reason) {
    if (g_phase == Phase::NewSave || g_phase == Phase::Generating) {
        g_status = "Could not connect: " + reason;
    } else if (g_phase == Phase::Playing) {
        toast("Archipelago", "Disconnected: " + reason, "warning", 4000);
    }
}

// ---------------------------------------------------------------------------------------
// ItemService callbacks

bool resolve_check(ModContext*, const ItemCheckInfo* info, ItemCheckResolution*, void*) {
    // Runs after the randomizer's resolver; only remembers where the last AP item came from.
    if (info->current_item == kApItem) {
        const auto locs = locations_for_check(info->name);
        if (!locs.empty()) {
            g_lastResolvedApLocation = locs.front();
        }
    }
    return false;
}

void report_locations(const std::vector<std::string>& locs) {
    for (const auto& loc : locs) {
        const auto it = g_locationIds.find(loc);
        ap_log(fmt::format("report_location '{}' known={}", loc, it != g_locationIds.end()));
        {
            if (!g_checked.contains(it->second)) {
                g_toSend.push_back(it->second);
                g_checked.insert(it->second);
            }
        }
    }
}

void observe_give(ModContext*, const ItemGiveInfo* info, void*) {
    std::string where = "(none)";
    std::string expected = "-";
    if (info->check_name != nullptr) {
        where = info->check_name;
        const auto locs = locations_for_check(info->check_name);
        if (!locs.empty()) {
            where += " -> " + locs.front();
            if (const auto e = g_expected.find(locs.front()); e != g_expected.end()) {
                expected = e->second;
            }
        }
    }
    ap_log(fmt::format("give item=0x{:02X} ({}) origin={} check='{}' ap_expects='{}'", info->item,
        item_name(info->item), info->origin, where, expected));
    if (info->check_name != nullptr) {
        const auto locs = locations_for_check(info->check_name);
        report_locations(locs);
        if (info->item == kApItem && !locs.empty()) {
            g_lastResolvedApLocation = locs.front();
        }
        return;
    }
    if ((info->origin == ITEM_GIVE_ORIGIN_QUEUE || info->origin == ITEM_GIVE_ORIGIN_QUEUE_SILENT) &&
        g_outstanding >= 0)
    {
        g_outstanding = -1;
        ++g_state.received;
        write_state();
    }
}

bool ap_item_text(ModContext*, const MessageOverrideContext*, MessageTextData* out, void*) {
    if (g_armedFrames <= 0 || g_armedText.empty()) {
        return false;
    }
    // Sanitized again at the sink: whatever the source, the renderer only ever sees printable
    // text of a sane length.
    const std::string text = message_safe(g_armedText, 160);
    thread_local std::vector<uint8_t> buf;
    buf.assign(text.begin(), text.end());
    buf.push_back(0);
    out->text = buf.data();
    out->text_size = buf.size();
    // Consume it: the next Archipelago item re-arms with its own text, and a real Foolish
    // Item (which shares this message) must not inherit it.
    g_armedFrames = 0;
    return true;
}

// ---------------------------------------------------------------------------------------
// Hooks: AP item model scale + goal detection

DEFINE_HOOK(&daDitem_c::set_mtx, ApDitemSetMtx);
DEFINE_HOOK(&daItem_c::setBaseMtx, ApItemSetBaseMtx);
DEFINE_HOOK(dStage_changeScene, ApChangeScene);
// Ganondorf's execute is file-local; hook it by translation unit alias.
DEFINE_HOOK_SYMBOL("src/d/actor/d_a_b_gnd.cpp#daB_GND_Execute", int(b_gnd_class*), ApGanondorf);

float model_scale() {
    double s = 0.6;
    if (g_cfgModelScale != 0) {
        svc_mng.config->get_float(svc_mng.mod_ctx, g_cfgModelScale, &s);
    }
    return static_cast<float>(s);
}

void scale_model(J3DModel* model) {
    if (model == nullptr) {
        return;
    }
    Mtx m;
    std::memcpy(m, model->getBaseTRMtx(), sizeof(Mtx));
    const float s = model_scale();
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            m[r][c] *= s;
        }
    }
    model->setBaseTRMtx(m);
}

void post_ditem_set_mtx(ModContext*, void* args, void*, void*) {
    auto* item = mods::arg<daDitem_c*>(args, 0);
    if (item->getDisplayItemNo() == kApItem) {
        scale_model(item->mpModel);
    }
}

void post_item_set_base_mtx(ModContext*, void* args, void*, void*) {
    auto* item = mods::arg<daItem_c*>(args, 0);
    if (item->getDisplayItemNo() == kApItem) {
        scale_model(item->mpModel);
    }
}

void complete_goal(const char* how) {
    if (g_state.goal) {
        return;
    }
    g_state.goal = true;
    write_state();
    g_client.sendGoal();
    ap_log(fmt::format("goal complete ({})", how));
    toast("Archipelago", "Goal complete!", "success", 6000);
}

void post_ganondorf_execute(ModContext*, void* args, void*, void*) {
    // The ending plays out inside the arena (no stage or scene change), so watch Ganondorf
    // himself: the finishing blow puts him in the end action with the ending camera running.
    constexpr s16 kActionEnd = 22;   // ACTION_END in d_a_b_gnd.cpp
    constexpr s16 kEndingCamera = 60;
    auto* gnd = mods::arg<b_gnd_class*>(args, 0);
    if (gnd != nullptr && gnd->mActionMode == kActionEnd && gnd->mDemoCamMode >= kEndingCamera) {
        complete_goal("Ganondorf defeated");
    }
}

HookAction pre_change_scene(ModContext*, void*, void*, void*) {
    const char* stage = dComIfGp_getStartStageName();
    ap_log(fmt::format("changeScene from stage '{}'", stage != nullptr ? stage : "?"));
    // Leaving Dark Lord Ganondorf's arena only happens through the ending.
    if (g_phase == Phase::Playing && stage != nullptr && std::strcmp(stage, "D_MN09C") == 0) {
        complete_goal("left D_MN09C");
    }
    return HOOK_CONTINUE;
}

// ---------------------------------------------------------------------------------------
// Per-frame work

bool in_gameplay() {
    return g_phase == Phase::Playing && !g_needsRegen && randomizer_IsActive() &&
           !playerIsOnTitleScreen() && dComIfGp_getPlayer(0) != nullptr;
}

void log_stage_changes() {
    static std::string last;
    const char* stage = dComIfGp_getStartStageName();
    if (stage != nullptr && last != stage) {
        last = stage;
        ap_log(fmt::format("stage -> {}", last));
    }
}

void scan_locations() {
    if (!g_haveSlot || g_scan.empty() || !in_gameplay()) {
        return;
    }
    for (int n = 0; n < 24; ++n) {
        const auto& loc = g_scan[g_scanPos];
        g_scanPos = (g_scanPos + 1) % g_scan.size();
        if (g_checked.contains(loc.id)) {
            continue;
        }
        if (loc.meta.IsMap() && isLocationMetadataObtained(loc.meta, loc.name)) {
            g_toSend.push_back(loc.id);
            g_checked.insert(loc.id);
        }
    }
}

void flush_checks() {
    if (!g_toSend.empty() && g_client.state() == State::Connected) {
        ap_log(fmt::format("sending {} location check(s)", g_toSend.size()));
        g_client.sendLocations(g_toSend);
        g_toSend.clear();
    }
}

void deliver_items() {
    if (!in_gameplay() || g_outstanding >= 0 ||
        g_state.received >= static_cast<int>(g_serverItems.size()))
    {
        return;
    }
    const auto& it = g_serverItems[g_state.received];
    const int64_t id = it.item - kItemIdBase;
    if (id < 0 || id > 0xFE) {
        ++g_state.received;  // not a giveable item for this game; skip
        write_state();
        return;
    }
    const auto give = static_cast<uint8_t>(verifyProgressiveItem(static_cast<u32>(id)));
    const bool progression = (it.flags & 1) != 0;
    const bool fromOther = it.player != g_client.slot() && it.location >= 0;
    const uint32_t flags = (progression && fromOther) ? 0u : ITEM_GIVE_SILENT;
    if (svc_mng.item->give_item(svc_mng.mod_ctx, nullptr, give, flags) != MOD_OK) {
        return;
    }
    g_outstanding = give;
    if (flags & ITEM_GIVE_SILENT) {
        const std::string from = g_client.playerName(it.player);
        toast("Received item", fmt::format("{}{}", item_name(give),
                                   fromOther && !from.empty() ? " from " + from : ""),
            nullptr, 3000);
    }
}

void tick_generation() {
    const int st = g_genStatus.load();
    if (st < 2) {
        return;
    }
    g_genStatus = 0;
    std::string hash, error;
    {
        std::lock_guard lock{g_genMutex};
        hash = g_genHash;
        error = g_genError;
    }
    if (st == 3) {
        mods::log::error("archipelago: seed generation failed: {}", error);
        g_status = "Seed generation failed: " + error;
        if (g_needsRegen) {
            toast("Archipelago", g_status, "warning", 10000);
        }
        g_phase = g_needsRegen ? Phase::Playing : Phase::NewSave;
        return;
    }
    if (g_needsRegen) {
        // Loaded save on a machine without its seed files: activate the rebuilt seed now.
        if (hash != g_loadedHash) {
            toast("Archipelago", "Rebuilt seed doesn't match this save.", "warning", 10000);
        } else {
            randomizer::session::deactivateSeed();
            randomizer::session::activateSeed(hash.c_str());
            loadAncientDocumentNum();
            g_needsRegen = false;
            after_seed_activated();
        }
        g_phase = Phase::Playing;
        return;
    }
    randomizer::session::g_pending_seed_hash = hash;
    g_status = "Seed ready!";
    g_phase = Phase::AwaitNewSave;
    randomizer::ui::g_file_select_window_ctx.is_proceed = true;  // continue to name entry
    mDoAud_seStartMenu(Z2SE_SY_NEW_FILE);
    if (g_window != 0) {
        const auto w = g_window;
        g_window = 0;
        svc_mng.ui->window_close(svc_mng.mod_ctx, w);
    }
}

void after_seed_activated() {
    // Our resolver must run after the randomizer's (registered on seed activation), so it sees
    // the resolved "Archipelago Item"; message overrides likewise stack on top of its text.
    if (g_resolver != 0) {
        svc_mng.item->clear_check_resolver(svc_mng.mod_ctx, g_resolver);
        g_resolver = 0;
    }
    svc_mng.item->set_check_resolver(svc_mng.mod_ctx, nullptr, resolve_check, nullptr, &g_resolver);
    g_textOverrides.clear();
    for (auto lang : {MESSAGE_LANGUAGE_ENGLISH, MESSAGE_LANGUAGE_GERMAN, MESSAGE_LANGUAGE_FRENCH,
             MESSAGE_LANGUAGE_SPANISH, MESSAGE_LANGUAGE_ITALIAN, MESSAGE_LANGUAGE_JAPANESE})
    {
        g_textOverrides.push_back(
            mods::flow::override_message_fn(0, kApItemDonorMessage, lang, ap_item_text));
    }
}

// ---------------------------------------------------------------------------------------
// New-save window

std::string g_inServer, g_inSlot, g_inPassword;

void get_str(ModContext*, void* ud, UiControlValue* out) {
    out->string_value = static_cast<std::string*>(ud)->c_str();
}
void set_str(ModContext*, void* ud, const UiControlValue* v) {
    *static_cast<std::string*>(ud) = v->string_value != nullptr ? v->string_value : "";
}

void press_connect(ModContext*, void*) {
    if (g_phase != Phase::NewSave) {
        return;
    }
    if (g_inSlot.empty()) {
        g_status = "Enter your slot (player) name.";
        return;
    }
    mDoAud_seStartMenu(Z2SE_SY_MENU_NEXT);
    g_conn = {g_inServer, g_inSlot, g_inPassword};
    if (g_cfgServer != 0) {
        svc_mng.config->set_string(svc_mng.mod_ctx, g_cfgServer, g_inServer.c_str());
        svc_mng.config->set_string(svc_mng.mod_ctx, g_cfgSlot, g_inSlot.c_str());
    }
    g_status = "Connecting...";
    g_client.connect({g_conn.server, g_conn.slot, g_conn.password});
}

bool connect_disabled(ModContext*, void*) {
    return g_phase != Phase::NewSave || g_client.state() == State::Connecting ||
           g_client.state() == State::Handshaking;
}

ModResult build_new_save_tab(ModContext* ctx, UiWindowHandle, UiElementHandle left, UiElementHandle,
    void*, ModError*) {
    auto string_row = [&](const char* label, std::string* target, const char* help) {
        UiControlDesc d = UI_CONTROL_DESC_INIT;
        d.kind = UI_CONTROL_STRING;
        d.label = label;
        d.help_rml = help;
        d.get = get_str;
        d.set = set_str;
        d.user_data = target;
        d.string_set_mode = UI_STRING_SET_ON_CHANGE;
        svc_mng.ui->pane_add_control(ctx, left, &d, nullptr);
    };
    svc_mng.ui->pane_add_section(ctx, left, "Archipelago");
    string_row("Server", &g_inServer, "Host and port, e.g. archipelago.gg:38281 or localhost:38281.");
    string_row("Slot name", &g_inSlot, "Your player name from your YAML.");
    string_row("Password", &g_inPassword, "Leave empty if the room has no password.");
    UiControlDesc b = UI_CONTROL_DESC_INIT;
    b.kind = UI_CONTROL_BUTTON;
    b.label = "Connect and start";
    b.on_pressed = press_connect;
    b.is_disabled = connect_disabled;
    svc_mng.ui->pane_add_control(ctx, left, &b, nullptr);
    g_statusText = 0;
    svc_mng.ui->pane_add_text(ctx, left, g_status.c_str(), &g_statusText);
    return MOD_OK;
}

ModResult update_new_save_tab(ModContext* ctx, void*, ModError*) {
    if (g_statusText != 0) {
        static std::string shown;
        if (shown != g_status) {
            shown = g_status;
            svc_mng.ui->elem_set_text(ctx, g_statusText, g_status.c_str());
        }
    }
    return MOD_OK;
}

void* g_fileSelect = nullptr;

void new_save_window_closed(ModContext*, UiWindowHandle, void*) {
    g_window = 0;
    g_statusText = 0;
    randomizer::ui::g_dialogSelectModeState = randomizer::ui::SelectReady;
    if (randomizer::ui::g_file_select_window_ctx.is_proceed) {
        return;
    }
    // Backed out: return the file-select menu to the data list, and drop the connection.
    if (auto* fs = static_cast<dFile_select_c*>(g_fileSelect)) {
        fs->headerTxtSet(0x43, 1, 0);
        fs->fileRecScaleAnmInitSet2(0.0f, 1.0f);
        fs->nameMoveAnmInitSet(0xd29, 0xd1f);
        fs->modoruTxtDispAnmInit(0);
        fs->mDataSelProc = dFile_select_c::DATASELPROC_NAME_TO_DATA_SELECT_MOVE;
    }
    g_client.disconnect();
    g_phase = Phase::Idle;
}

ModResult open_gate_window(void* fileSelect) {
    g_fileSelect = fileSelect;
    randomizer::ui::g_file_select_window_ctx.is_proceed = false;
    g_phase = Phase::NewSave;
    g_haveSlot = false;
    g_serverItems.clear();
    g_status = "Connect to your Archipelago room. Your seed is built from the server's data.";
    g_inServer = config_string(g_cfgServer);
    if (g_inServer.empty()) {
        g_inServer = "archipelago.gg:38281";
    }
    g_inSlot = config_string(g_cfgSlot);
    g_inPassword.clear();

    static UiTabDesc tab = UI_TAB_DESC_INIT;
    tab.title = "Archipelago";
    tab.build = build_new_save_tab;
    tab.update = update_new_save_tab;
    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = &tab;
    desc.tab_count = 1;
    desc.on_closed = new_save_window_closed;
    return svc_mng.ui->window_push(svc_mng.mod_ctx, &desc, &g_window);
}

// ---------------------------------------------------------------------------------------
// Save lifecycle

ModResult on_new_save(void* ud, ModError* err) {
    const ModResult r = randomizer::session::onNewSave(ud, err);
    if (r != MOD_OK) {
        return r;
    }
    g_state = {};
    g_state.seed = g_slotSeed;
    g_state.slot = g_conn.slot;
    write_state();
    write_conn();
    g_loadedHash = randomizer_GetContext().mHash;
    g_needsRegen = false;
    g_outstanding = -1;
    g_phase = Phase::Playing;
    ap_log(fmt::format("new save created, hash {}, scan list {}", g_loadedHash, g_scan.size()));
    after_seed_activated();
    return MOD_OK;
}

ModResult on_save_loaded(void* ud, ModError* err) {
    g_outstanding = -1;
    g_toSend.clear();
    Conn conn;
    SaveState state;
    const bool haveConn = read_blob(kConnBlob, conn);
    read_blob(kStateBlob, state);

    std::string hash;
    {
        size_t size = 0;
        if (svc_mng.save->get_blob(svc_mng.mod_ctx, kSeedHashBlob, nullptr, &size) == MOD_OK && size) {
            hash.resize(size);
            svc_mng.save->get_blob(svc_mng.mod_ctx, kSeedHashBlob, hash.data(), &size);
        }
    }
    g_loadedHash = hash;
    g_needsRegen = !seed_files_exist(hash);
    if (!g_needsRegen) {
        const ModResult r = randomizer::session::onSaveLoaded(ud, err);
        if (r != MOD_OK) {
            return r;
        }
        after_seed_activated();
    } else {
        randomizer::session::deactivateSeed();
    }

    const bool sameSession = g_client.state() == State::Connected && g_haveSlot &&
                             g_state.seed == state.seed && g_conn.slot == conn.slot;
    g_state = state;
    g_phase = Phase::Playing;
    if (!haveConn) {
        toast("Archipelago", "This save has no Archipelago connection info.", "warning");
        return MOD_OK;
    }
    g_conn = conn;
    if (!sameSession) {
        g_serverItems.clear();
        g_haveSlot = false;
        g_client.connect({g_conn.server, g_conn.slot, g_conn.password});
    } else if (g_needsRegen && !g_lastSlotData.is_null()) {
        g_phase = Phase::Generating;
        start_generation(g_lastSlotData, g_conn.slot);
    }
    return MOD_OK;
}

ModResult on_game_reset(void*, ModError*) {
    g_phase = Phase::Idle;
    g_outstanding = -1;
    return MOD_OK;
}

// ---------------------------------------------------------------------------------------
// Status window (menu bar tab)

std::string g_editServer;

void press_reconnect(ModContext*, void*) {
    if (!g_editServer.empty() && g_editServer != g_conn.server) {
        g_conn.server = g_editServer;
        if (g_phase == Phase::Playing) {
            write_conn();
        }
    }
    if (!g_conn.slot.empty()) {
        g_client.connect({g_conn.server, g_conn.slot, g_conn.password});
    }
}

void press_disconnect(ModContext*, void*) {
    g_client.disconnect();
}

std::string status_text() {
    std::string s = status_line();
    if (g_phase == Phase::Playing) {
        s += fmt::format("\nItems received: {} / {}", g_state.received, g_serverItems.size());
        if (g_haveSlot) {
            s += fmt::format("\nChecks sent: {} / {}", g_checked.size(), g_locationIds.size());
        }
        if (g_state.goal) {
            s += "\nGoal complete!";
        }
        if (g_needsRegen) {
            s += "\nWaiting to rebuild this save's seed from the server.";
        }
    }
    return s;
}

ModResult build_status_tab(ModContext* ctx, UiWindowHandle, UiElementHandle left, UiElementHandle,
    void*, ModError*) {
    g_editServer = g_conn.server;
    svc_mng.ui->pane_add_section(ctx, left, "Connection");
    g_statusWindowText = 0;
    svc_mng.ui->pane_add_text(ctx, left, status_text().c_str(), &g_statusWindowText);
    UiControlDesc d = UI_CONTROL_DESC_INIT;
    d.kind = UI_CONTROL_STRING;
    d.label = "Server";
    d.help_rml = "Change this if the room moved to a new port, then press Reconnect.";
    d.get = get_str;
    d.set = set_str;
    d.user_data = &g_editServer;
    svc_mng.ui->pane_add_control(ctx, left, &d, nullptr);
    UiControlDesc b = UI_CONTROL_DESC_INIT;
    b.kind = UI_CONTROL_BUTTON;
    b.label = "Reconnect";
    b.on_pressed = press_reconnect;
    svc_mng.ui->pane_add_control(ctx, left, &b, nullptr);
    UiControlDesc goal = UI_CONTROL_DESC_INIT;
    goal.kind = UI_CONTROL_BUTTON;
    goal.label = "Send goal complete";
    goal.help_rml = "Tells the server you finished the game, in case it wasn't detected.";
    goal.on_pressed = [](ModContext*, void*) { complete_goal("manual"); };
    goal.is_disabled = [](ModContext*, void*) {
        return g_state.goal || g_client.state() != State::Connected;
    };
    svc_mng.ui->pane_add_control(ctx, left, &goal, nullptr);
    UiControlDesc b2 = UI_CONTROL_DESC_INIT;
    b2.kind = UI_CONTROL_BUTTON;
    b2.label = "Disconnect";
    b2.on_pressed = press_disconnect;
    svc_mng.ui->pane_add_control(ctx, left, &b2, nullptr);
    return MOD_OK;
}

ModResult update_status_tab(ModContext* ctx, void*, ModError*) {
    if (g_statusWindowText != 0) {
        static std::string shown;
        const std::string now = status_text();
        if (now != shown) {
            shown = now;
            svc_mng.ui->elem_set_text(ctx, g_statusWindowText, now.c_str());
        }
    }
    return MOD_OK;
}

void open_status_window(ModContext*, void*) {
    static UiTabDesc tab = UI_TAB_DESC_INIT;
    tab.title = "Status";
    tab.build = build_status_tab;
    tab.update = update_status_tab;
    UiWindowDesc desc = UI_WINDOW_DESC_INIT;
    desc.tabs = &tab;
    desc.tab_count = 1;
    desc.on_closed = [](ModContext*, UiWindowHandle, void*) {
        g_statusWindow = 0;
        g_statusWindowText = 0;
    };
    svc_mng.ui->window_push(svc_mng.mod_ctx, &desc, &g_statusWindow);
}

// Auto-reconnect
int g_reconnectFrames = 0;

}  // namespace

// =========================================================================================

GameModeDesc game_mode_desc() {
    return GameModeDesc{
        .struct_size = sizeof(GameModeDesc),
        .game_mode_id = kGameModeId,
        .full_name = "Archipelago",
        .save_name = "archipelago",
        .user_data = nullptr,
        .on_save_loaded = on_save_loaded,
        .on_new_save = on_new_save,
        .on_game_reset = on_game_reset,
    };
}

ModResult activate() {
    g_client.onConnected = on_connected;
    g_client.onItems = on_items;
    g_client.onPrint = on_print;
    g_client.onDisconnected = on_disconnected;

    auto reg_string = [](const char* name, ConfigVarHandle& out) {
        ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
        d.name = name;
        d.type = CONFIG_VAR_STRING;
        d.default_string = "";
        svc_mng.config->register_var(svc_mng.mod_ctx, &d, &out);
    };
    if (g_cfgServer == 0) {
        reg_string("lastServer", g_cfgServer);
        reg_string("lastSlot", g_cfgSlot);
        ConfigVarDesc d = CONFIG_VAR_DESC_INIT;
        d.name = "apItemModelScale";
        d.type = CONFIG_VAR_FLOAT;
        d.default_float = 0.6;
        svc_mng.config->register_var(svc_mng.mod_ctx, &d, &g_cfgModelScale);
        ConfigVarDesc debug = CONFIG_VAR_DESC_INIT;
        debug.name = "debugLog";
        debug.type = CONFIG_VAR_BOOL;
        debug.default_bool = false;
        svc_mng.config->register_var(svc_mng.mod_ctx, &debug, &g_cfgDebugLog);
    }

    svc_mng.item->observe_gives(svc_mng.mod_ctx, observe_give, nullptr, &g_observer);

    if (mods::hook::add_post<ApDitemSetMtx>(post_ditem_set_mtx) != MOD_OK ||
        mods::hook::add_post<ApItemSetBaseMtx>(post_item_set_base_mtx) != MOD_OK ||
        mods::hook::add_post<ApGanondorf>(post_ganondorf_execute) != MOD_OK ||
        mods::hook::add_pre<ApChangeScene>(pre_change_scene) != MOD_OK)
    {
        mods::log::error("archipelago: failed to install hooks");
        return MOD_ERROR;
    }

    UiMenuTabDesc tab = UI_MENU_TAB_DESC_INIT;
    tab.label = "Archipelago";
    tab.on_selected = open_status_window;
    svc_mng.ui->register_menu_tab(svc_mng.mod_ctx, &tab, &g_menuTab);
    return MOD_OK;
}

void deactivate() {
    g_client.disconnect();
    g_textOverrides.clear();
    if (g_resolver != 0) {
        svc_mng.item->clear_check_resolver(svc_mng.mod_ctx, g_resolver);
        g_resolver = 0;
    }
    if (g_observer != 0) {
        svc_mng.item->unobserve_gives(svc_mng.mod_ctx, g_observer);
        g_observer = 0;
    }
    mods::hook::uninstall<ApDitemSetMtx>();
    mods::hook::uninstall<ApItemSetBaseMtx>();
    mods::hook::uninstall<ApChangeScene>();
    mods::hook::uninstall<ApGanondorf>();
    if (g_menuTab != 0) {
        svc_mng.ui->unregister_menu_tab(svc_mng.mod_ctx, g_menuTab);
        g_menuTab = 0;
    }
    g_phase = Phase::Idle;
}

void update() {
    g_client.poll();
    tick_generation();
}

void tick() {
    if (g_armedFrames > 0) {
        --g_armedFrames;
    }

    if (g_phase == Phase::Playing) {
        // Keep an AP save connected: retry every ~10 s after a drop (not after a refusal).
        if (g_client.state() == State::Disconnected && !g_conn.slot.empty()) {
            if (++g_reconnectFrames > 600) {
                g_reconnectFrames = 0;
                g_client.connect({g_conn.server, g_conn.slot, g_conn.password});
            }
        } else {
            g_reconnectFrames = 0;
        }
        log_stage_changes();
        scan_locations();
        flush_checks();
        deliver_items();
    }
}

ModResult open_connect_gate(void* fileSelect) {
    return open_gate_window(fileSelect);
}

void on_get_item_demo(void* link) {
    auto* alink = static_cast<daAlink_c*>(link);
    if (alink->field_0x32cc != 0) {
        return;
    }
    if (alink->mProcVar2.field_0x300c != kApItem) {
        if (alink->mProcVar2.field_0x300c == dItemNo_Randomizer_FOOLISH_ITEM_e) {
            g_armedFrames = 0;  // a real Foolish Item uses the same message
        }
        return;
    }
    // Always re-arm from the check being collected right now; the placeholder item has no
    // message of its own, so it must never fall back to whatever was shown last.
    g_armedText.clear();
    if (!g_lastResolvedApLocation.empty()) {
        if (const auto it = g_apItemText.find(g_lastResolvedApLocation); it != g_apItemText.end()) {
            g_armedText = it->second;
        }
    }
    if (g_armedText.empty()) {
        g_armedText = "You found another player's item!";
    }
    g_armedFrames = 600;
    alink->field_0x32cc = kApItemDonorMessage;
}

}  // namespace ap
