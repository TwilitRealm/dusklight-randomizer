#include "hints.hpp"

#include "dusk/randomizer/generator/utility/text.hpp"
#include "world.hpp"

namespace randomizer::logic::hints {

    // Tell the player which dungeons are required on the sign in front of Link's House
    static void GenerateRequiredDungeonsHint(world::WorldPool& worlds) {
        static const std::unordered_map<std::string, std::string> dungeonColors = {
            {"Forest Temple", "<green>"},
            {"Goron Mines", "<red>"},
            {"Lakebed Temple", "<blue>"},
            {"Arbiters Grounds", "<orange>"},
            {"Snowpeak Ruins", "<light blue>"},
            {"Temple of Time", "<dark green>"},
            {"City in the Sky", "<yellow>"},
            {"Palace of Twilight", "<purple>"},
            // {"Hyrule Castle", "<silver>"}
        };

        for (const auto& world : worlds) {
            auto& requiredDungeonText = world->AddDynamicTextStr("Links House Sign");
            for (const auto& [name, dungeon] : world->GetDungeonTable()) {
                if (dungeon->IsRequired()) {
                    requiredDungeonText += dungeonColors.at(name) + name + "\n";
                }
            }
        }
    }

    void GenerateAllHints(world::WorldPool& worlds) {
        GenerateRequiredDungeonsHint(worlds);
    }
}