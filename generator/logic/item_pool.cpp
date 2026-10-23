#include "item_pool.hpp"

#include "world.hpp"

#include <map>
#include <mutex>

namespace randomizer::logic::item_pool
{
    std::once_flag poolsInitialized;

    std::map<std::string, int> minimalItemPool;

    // This is intended to be added on top of the minimal item pool
    std::map<std::string, int> standardItemPool;

    // This is intended to be added on top of the minimal and standard pools
    std::map<std::string, int> plentifulItemPool;

    std::map<std::string, int> initialJunkPool;

    std::map<std::string, int> parsePool(YAML::Node const& node) {
        std::map<std::string, int> result;

        for (auto const& iter : node) {
            result[iter.first.as<std::string>()] = iter.second.as<int>();
        }

        return result;
    }

    void ensurePoolsInitialized() {
        std::call_once(poolsInitialized, [] {
            auto poolData = LOAD_EMBED_YAML(RANDO_DATA_PATH "pools.yaml");

            minimalItemPool = parsePool(poolData["Minimal"]);
            standardItemPool = parsePool(poolData["Standard"]);
            plentifulItemPool = parsePool(poolData["Plentiful"]);
            initialJunkPool = parsePool(poolData["Junk"]);
        });
    }

    void GenerateItemPool(world::World* world)
    {
        ensurePoolsInitialized();

        auto itemPool = minimalItemPool;

        // Minimal item pool things
        if (world->Setting("Item Scarcity") == "Minimal")
        {
            // If glitched logic, include magic armor
        }

        // Add the vanilla item pool if necessary
        if (world->Setting("Item Scarcity").IsAnyOf("Vanilla", "Plentiful"))
        {
            for (const auto& [itemName, count] : standardItemPool)
            {
                itemPool[itemName] += count;
            }
        }

        // Add the plentiful item pool if necessary
        if (world->Setting("Item Scarcity") == "Plentiful")
        {
            for (const auto& [itemName, count] : plentifulItemPool)
            {
                itemPool[itemName] += count;
            }
        }

        // Remove the Tears for Twilight sections if they're cleared
        if (world->Setting("Faron Twilight Cleared") == "On") {
            itemPool.erase("Faron Twilight Tear");
        }

        if (world->Setting("Eldin Twilight Cleared") == "On") {
            itemPool.erase("Eldin Twilight Tear");
        }

        if (world->Setting("Lanayru Twilight Cleared") == "On") {
            itemPool.erase("Lanayru Twilight Tear");
        }

        // Remove items depending on Ilia Memory Quest setting
        const auto& iliaQuest = world->Setting("Ilia Memory Quest");
        if (iliaQuest > "Letter") {
            itemPool.erase("Renados Letter");
        }
        if (iliaQuest > "Invoice") {
            itemPool.erase("Invoice");
        }
        if (iliaQuest > "Statue") {
            itemPool.erase("Wooden Statue");
        }

        // Remove the North Faron Woods Gate Key if we're skipping prologue
        if (world->Setting("Skip Prologue") == "On")
        {
            itemPool.erase("North Faron Woods Gate Key");
        }

        // Remove the bulblin camp key if we're skipping bulblin camp
        if (world->Setting("Arbiters Does Not Require Bulblin Camp") == "On")
        {
            itemPool.erase("Gerudo Desert Bulblin Camp Key");
        }

        // Remove all but 1 sky book characters if we're starting with the sky cannon open
        if (world->Setting("City Does Not Require Filled Skybook") == "On")
        {
            itemPool["Progressive Sky Book"] = 1;
        }

        // Remove Small Keys if we're playing without them
        if (world->Setting("Small Keys") == "Keysy")
        {
            std::list<std::string> smallKeys = {
                {"Gate Keys"},
                {"Gerudo Desert Bulblin Camp Key"},
                {"Faron Woods Coro Key"},
                {"Forest Temple Small Key"},
                {"Goron Mines Small Key"},
                {"Lakebed Temple Small Key"},
                {"Arbiters Grounds Small Key"},
                {"Snowpeak Ruins Small Key"},
                {"Ordon Pumpkin"},
                {"Ordon Cheese"},
                {"Temple of Time Small Key"},
                {"City in the Sky Small Key"},
                {"Palace of Twilight Small Key"},
                {"Hyrule Castle Small Key"},
            };
            for (const auto& key : smallKeys)
            {
                itemPool.erase(key);
            }
        }

        // Remove Big Keys if we're playing without them
        if (world->Setting("Big Keys") == "Keysy")
        {
            std::list<std::string> bigKeys = {
                {"Forest Temple Big Key"},
                {"Goron Mines Key Shard"},
                {"Lakebed Temple Big Key"},
                {"Arbiters Grounds Big Key"},
                {"Snowpeak Ruins Bedroom Key"},
                {"Temple of Time Big Key"},
                {"City in the Sky Big Key"},
                {"Palace of Twilight Big Key"},
            };

            if (world->Setting("Hyrule Castle Big Key Requirements") == "None")
            {
                bigKeys.emplace_back("Hyrule Castle Big Key");
            }

            for (const auto& key : bigKeys)
            {
                itemPool.erase(key);
            }
        }

        // Add items to the world's _itemPool
        auto& worldItemPool = world->GetItemPool();
        for (const auto& [itemName, count] : itemPool)
        {
            auto item = world->GetItem(itemName);
            for (auto i = 0; i < count; i++)
            {
                worldItemPool.push_back(item);
            }
        }
    }

    void GenerateStartingItemPool(world::World* world)
    {
        ensurePoolsInitialized();

        auto startingItems = world->GetSettings().GetStartingInventory();
        auto& startingItemPool = world->GetStartingItemPool();
        auto& itemPool = world->GetItemPool();

        // Add Maps and Compasses to starting items if we start with them
        if (world->Setting("Maps and Compasses") == "Start With")
        {
            std::list<std::string> mapsAndCompasses = {
                {"Forest Temple Compass"},
                {"Goron Mines Compass"},
                {"Lakebed Temple Compass"},
                {"Arbiters Grounds Compass"},
                {"Snowpeak Ruins Compass"},
                {"Temple of Time Compass"},
                {"City in the Sky Compass"},
                {"Palace of Twilight Compass"},
                {"Hyrule Castle Compass"},
                {"Forest Temple Dungeon Map"},
                {"Goron Mines Dungeon Map"},
                {"Lakebed Temple Dungeon Map"},
                {"Arbiters Grounds Dungeon Map"},
                {"Snowpeak Ruins Dungeon Map"},
                {"Temple of Time Dungeon Map"},
                {"City in the Sky Dungeon Map"},
                {"Palace of Twilight Dungeon Map"},
                {"Hyrule Castle Dungeon Map"},
            };

            for (const auto& itemName : mapsAndCompasses)
            {
                startingItems[itemName] = 1;
            }
        }

        // Handle warp portals
        startingItems["Ordon Spring Portal"] = 1;
        if (world->Setting("Faron Twilight Cleared") == "On")
        {
            startingItems["South Faron Portal"] = 1;
            startingItems["North Faron Portal"] = 1;
        }

        if (world->Setting("Eldin Twilight Cleared") == "On")
        {
            startingItems["Kakariko Gorge Portal"] = 1;
            startingItems["Kakariko Village Portal"] = 1;
            startingItems["Death Mountain Portal"] = 1;
        }

        if (world->Setting("Lanayru Twilight Cleared") == "On")
        {
            startingItems["Zoras Domain Portal"] = 1;
            startingItems["Lake Hylia Portal"] = 1;
            startingItems["Castle Town Portal"] = 1;
        }

        // Automatically give players the Mirror Chamber Portal if Mirror Chamber Access is closed
        // and they aren't both randomizing and decoupling dungeon entrances. Otherwise, there's no
        // way to access the chamber
        if (world->Setting("Mirror Chamber Access") == "Closed" &&
          !(world->Setting("Randomize Dungeon Entrances") == "On" && world->Setting("Decouple Entrances") == "On"))
        {
            startingItems["Mirror Chamber Portal"] = 1;
        }

        // Add each item to the world's _startingItemPool and erase it from the regular _itemPool
        for (const auto& [itemName, count] : startingItems)
        {
            auto item = world->GetItem(itemName);
            for (auto i = 0; i < count; i++)
            {
                startingItemPool.push_back(item);
            }
            utility::container::Erase(itemPool, item, count);
        }
    }

    std::map<std::string, int> GetInitialJunkPool()
    {
        ensurePoolsInitialized();

        return initialJunkPool;
    }

    ItemPool GetCompleteItemPool(const world::WorldPool& worlds)
    {
        ensurePoolsInitialized();

        ItemPool completeItemPool = {};
        for (const auto& world : worlds)
        {
            auto& worldItemPool = world->GetItemPool();
            std::ranges::copy(worldItemPool, std::back_inserter(completeItemPool));
        }

        return completeItemPool;
    }

    const std::map<std::string, int>& GetValidStartingInventoryItems() {
        ensurePoolsInitialized();

        static std::map<std::string, int> validStartingInventoryItems{};
        if (validStartingInventoryItems.empty()) {
            validStartingInventoryItems = minimalItemPool;
            for (const auto& [item, count] : standardItemPool) {
                validStartingInventoryItems[item] += count;
            }

            // Remove junk
            validStartingInventoryItems.erase("Purple Rupee Links House");
            validStartingInventoryItems.erase("Green Rupee");
            validStartingInventoryItems.erase("Orange Rupee");
            validStartingInventoryItems.erase("Silver Rupee");
        }
        return validStartingInventoryItems;
    }
} // namespace randomizer::logic::item_pool
