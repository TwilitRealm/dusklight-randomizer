#pragma once

#include "mods/svc/host.h"
#include "mods/svc/log.h"
#include "mods/svc/config.h"
#include "mods/svc/hook.h"
#include "mods/svc/ui.h"
#include "mods/svc/resource.h"
#include "mods/svc/save.h"
#include "mods/svc/stage.h"
#include "mods/svc/item.h"
#include "mods/svc/flow.h"
#include "mods/svc/message.h"
#include "mods/svc/game_mode.h"
#include "mods/svc/texture.h"

#include <dolphin/types.h>

#include <optional>
#include <string>
#include <string_view>

namespace randomizer::session {
struct ServiceManager {
    ModContext* mod_ctx;
    const HostService* host;
    const LogService* log;
    const HookService* hook;
    const UiService* ui;
    const ResourceService* resource;
    const ConfigService* config;
    const SaveService* save;
    const StageService* stage;
    const ItemService* item;
    const FlowService* flow;
    const MessageService* message;
    const GameModeService* game_mode;
    const TextureService* texture;
    const FileService* file;
};

extern ServiceManager svc_mng;
extern std::string g_pending_seed_hash;
extern bool g_seedActivated;

ModResult initialize(const ServiceManager& services);
void update();
void shutdown();

void deactivateSeed();
bool activateSeed(const char* hash);
ModResult onNewSave(void*, ModError*);
ModResult onSaveLoaded(void*, ModError*);

// Check-name parsing shared with the Archipelago module (ItemService check names -> seed keys).
struct DerivedKey {
    int stage_id;
    u16 key;
};
std::optional<int> parse_stage_check(const char* name, std::string_view prefix);
std::optional<DerivedKey> parse_derived(const char* name, std::string_view prefix);
std::optional<u32> parse_shop_check(const char* name, std::string_view prefix);
std::optional<u16> parse_flag_check(const char* name, std::string_view prefix);
void setupRandomizerFile();
void registerStageEdits();
}
