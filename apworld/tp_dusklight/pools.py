"""Ports of the randomizer's item pool and vanilla-placement rules (item_pool.cpp, world.cpp)."""
from __future__ import annotations

from collections import Counter
from typing import TYPE_CHECKING

from . import data

if TYPE_CHECKING:
    from . import TPWorld

MINIMAL_POOL = {
    "Shadow Crystal": 1, "Slingshot": 1, "Lantern": 1, "Gale Boomerang": 1, "Iron Boots": 1,
    "Bomb Bag": 1, "Spinner": 1, "Ball and Chain": 1,
    "Progressive Fishing Rod": 2, "Progressive Sword": 4, "Progressive Bow": 1,
    "Progressive Clawshot": 2, "Progressive Dominion Rod": 2, "Progressive Wallet": 2,
    "Progressive Sky Book": 7,
    "Aurus Memo": 1, "Asheis Sketch": 1, "Renados Letter": 1, "Invoice": 1, "Wooden Statue": 1,
    "Ilias Charm": 1, "Zora Armor": 1, "Hylian Shield": 1, "Ordon Shield": 1, "Empty Bottle": 4,
    "Progressive Hidden Skill": 1, "Poe Soul": 60,
    "Progressive Fused Shadow": 3, "Progressive Mirror Shard": 4,
    "Male Ant": 1, "Female Ant": 1, "Male Beetle": 1, "Female Beetle": 1, "Male Pill Bug": 1,
    "Female Pill Bug": 1, "Male Phasmid": 1, "Female Phasmid": 1, "Male Grasshopper": 1,
    "Female Grasshopper": 1, "Male Stag Beetle": 1, "Female Stag Beetle": 1, "Male Butterfly": 1,
    "Female Butterfly": 1, "Male Ladybug": 1, "Female Ladybug": 1, "Male Mantis": 1,
    "Female Mantis": 1, "Male Dragonfly": 1, "Female Dragonfly": 1, "Male Dayfly": 1,
    "Female Dayfly": 1, "Male Snail": 1, "Female Snail": 1,
    "Gate Keys": 1, "Gerudo Desert Bulblin Camp Key": 1, "North Faron Woods Gate Key": 1,
    "Faron Woods Coro Key": 1, "Forest Temple Small Key": 4, "Goron Mines Small Key": 3,
    "Lakebed Temple Small Key": 3, "Arbiters Grounds Small Key": 5, "Snowpeak Ruins Small Key": 4,
    "Ordon Pumpkin": 1, "Ordon Cheese": 1, "Temple of Time Small Key": 3,
    "City in the Sky Small Key": 1, "Palace of Twilight Small Key": 7, "Hyrule Castle Small Key": 3,
    "Forest Temple Big Key": 1, "Goron Mines Key Shard": 3, "Lakebed Temple Big Key": 1,
    "Arbiters Grounds Big Key": 1, "Snowpeak Ruins Bedroom Key": 1, "Temple of Time Big Key": 1,
    "City in the Sky Big Key": 1, "Palace of Twilight Big Key": 1, "Hyrule Castle Big Key": 1,
    "Forest Temple Compass": 1, "Goron Mines Compass": 1, "Lakebed Temple Compass": 1,
    "Arbiters Grounds Compass": 1, "Snowpeak Ruins Compass": 1, "Temple of Time Compass": 1,
    "City in the Sky Compass": 1, "Palace of Twilight Compass": 1, "Hyrule Castle Compass": 1,
    "Forest Temple Dungeon Map": 1, "Goron Mines Dungeon Map": 1, "Lakebed Temple Dungeon Map": 1,
    "Arbiters Grounds Dungeon Map": 1, "Snowpeak Ruins Dungeon Map": 1,
    "Temple of Time Dungeon Map": 1, "City in the Sky Dungeon Map": 1,
    "Palace of Twilight Dungeon Map": 1, "Hyrule Castle Dungeon Map": 1,
    "Ordon Spring Portal": 1, "South Faron Portal": 1, "North Faron Portal": 1,
    "Kakariko Gorge Portal": 1, "Kakariko Village Portal": 1, "Death Mountain Portal": 1,
    "Bridge of Eldin Portal": 1, "Zoras Domain Portal": 1, "Lake Hylia Portal": 1,
    "Castle Town Portal": 1, "Upper Zoras River Portal": 1, "Snowpeak Portal": 1,
    "Gerudo Desert Portal": 1, "Mirror Chamber Portal": 1,
    "Faron Twilight Tear": 16, "Eldin Twilight Tear": 16, "Lanayru Twilight Tear": 16,
    "Purple Rupee Links House": 1, "Green Rupee": 2, "Orange Rupee": 50, "Silver Rupee": 2,
}

STANDARD_POOL = {
    "Bomb Bag": 2, "Progressive Bow": 2, "Progressive Wallet": 1, "Magic Armor": 1, "Hawkeye": 1,
    "Giant Bomb Bag": 1, "Horse Call": 1, "Progressive Hidden Skill": 6,
    "Heart Container": 8, "Piece of Heart": 45,
}

PLENTIFUL_POOL = {
    "Shadow Crystal": 1, "Slingshot": 1, "Lantern": 1, "Gale Boomerang": 1, "Iron Boots": 1,
    "Bomb Bag": 1, "Spinner": 1, "Ball and Chain": 1,
    "Progressive Fishing Rod": 1, "Progressive Sword": 4, "Progressive Bow": 1,
    "Progressive Clawshot": 1, "Progressive Dominion Rod": 1, "Progressive Wallet": 1,
    "Progressive Sky Book": 1,
    "Aurus Memo": 1, "Asheis Sketch": 1, "Zora Armor": 1, "Magic Armor": 1, "Hylian Shield": 1,
    "Empty Bottle": 1, "Progressive Hidden Skill": 1,
    "Gate Keys": 1, "Forest Temple Small Key": 1, "Goron Mines Small Key": 1,
    "Lakebed Temple Small Key": 1, "Arbiters Grounds Small Key": 1, "Snowpeak Ruins Small Key": 1,
    "Ordon Pumpkin": 1, "Ordon Cheese": 1, "Temple of Time Small Key": 1,
    "City in the Sky Small Key": 1, "Palace of Twilight Small Key": 1, "Hyrule Castle Small Key": 1,
    "Forest Temple Big Key": 1, "Goron Mines Key Shard": 1, "Lakebed Temple Big Key": 1,
    "Arbiters Grounds Big Key": 1, "Snowpeak Ruins Bedroom Key": 1, "Temple of Time Big Key": 1,
    "City in the Sky Big Key": 1, "Palace of Twilight Big Key": 1, "Hyrule Castle Big Key": 1,
}

JUNK_POOL = {
    "Bombs 5": 8, "Bombs 10": 2, "Bombs 20": 1, "Bombs 30": 1, "Arrows 10": 13, "Arrows 20": 6,
    "Arrows 30": 2, "Seeds 50": 2, "Water Bombs 5": 3, "Water Bombs 10": 5, "Water Bombs 15": 3,
    "Bomblings 5": 2, "Bomblings 10": 2, "Blue Rupee": 1, "Yellow Rupee": 6, "Red Rupee": 6,
    "Purple Rupee": 12,
}

SMALL_KEYS_KEYSY = (
    "Gate Keys", "Gerudo Desert Bulblin Camp Key", "Faron Woods Coro Key", "Forest Temple Small Key",
    "Goron Mines Small Key", "Lakebed Temple Small Key", "Arbiters Grounds Small Key",
    "Snowpeak Ruins Small Key", "Ordon Pumpkin", "Ordon Cheese", "Temple of Time Small Key",
    "City in the Sky Small Key", "Palace of Twilight Small Key", "Hyrule Castle Small Key",
)

BIG_KEYS_KEYSY = (
    "Forest Temple Big Key", "Goron Mines Key Shard", "Lakebed Temple Big Key",
    "Arbiters Grounds Big Key", "Snowpeak Ruins Bedroom Key", "Temple of Time Big Key",
    "City in the Sky Big Key", "Palace of Twilight Big Key",
)

MAPS_AND_COMPASSES = tuple(
    f"{d} {kind}" for kind in ("Compass", "Dungeon Map") for d in data.DUNGEONS)

ALWAYS_VANILLA_NAMES = ("Renados Letter", "Telma Invoice", "Wooden Statue", "Ilia Charm",
                        "Defeat Ganondorf", "Twilit Insect", "Twilit Bloat")


def build_item_pool(world: "TPWorld") -> Counter:
    s = world.setting
    pool = Counter(MINIMAL_POOL)
    if s("Item Scarcity") in ("Vanilla", "Plentiful"):
        pool.update(STANDARD_POOL)
    if s("Item Scarcity") == "Plentiful":
        pool.update(PLENTIFUL_POOL)
    for region in ("Faron", "Eldin", "Lanayru"):
        if s(f"{region} Twilight Cleared") == "On":
            pool.pop(f"{region} Twilight Tear", None)
    ilia = world.setting_index("Ilia Memory Quest")
    idx = data.settings()["Ilia Memory Quest"].index_of
    if ilia > idx("Letter"):
        pool.pop("Renados Letter", None)
    if ilia > idx("Invoice"):
        pool.pop("Invoice", None)
    if ilia > idx("Statue"):
        pool.pop("Wooden Statue", None)
    if s("Skip Prologue") == "On":
        pool.pop("North Faron Woods Gate Key", None)
    if s("Arbiters Does Not Require Bulblin Camp") == "On":
        pool.pop("Gerudo Desert Bulblin Camp Key", None)
    if s("City Does Not Require Filled Skybook") == "On":
        pool["Progressive Sky Book"] = 1
    if s("Small Keys") == "Keysy":
        for k in SMALL_KEYS_KEYSY:
            pool.pop(k, None)
    if s("Big Keys") == "Keysy":
        keys = list(BIG_KEYS_KEYSY)
        if s("Hyrule Castle Big Key Requirements") == "None":
            keys.append("Hyrule Castle Big Key")
        for k in keys:
            pool.pop(k, None)
    return pool


def starting_items(world: "TPWorld") -> Counter:
    """GenerateStartingItemPool's randomizer-intrinsic starting items (excludes AP start inventory)."""
    s = world.setting
    start: Counter = Counter()
    if s("Maps and Compasses") == "Start With":
        for name in MAPS_AND_COMPASSES:
            start[name] = 1
    start["Ordon Spring Portal"] = 1
    if s("Faron Twilight Cleared") == "On":
        start["South Faron Portal"] = start["North Faron Portal"] = 1
    if s("Eldin Twilight Cleared") == "On":
        start["Kakariko Gorge Portal"] = start["Kakariko Village Portal"] = start["Death Mountain Portal"] = 1
    if s("Lanayru Twilight Cleared") == "On":
        start["Zoras Domain Portal"] = start["Lake Hylia Portal"] = start["Castle Town Portal"] = 1
    if s("Mirror Chamber Access") == "Closed" and not (
            s("Randomize Dungeon Entrances") == "On" and s("Decouple Entrances") == "On"):
        start["Mirror Chamber Portal"] = 1
    return start


def should_remove_location(world: "TPWorld", name: str) -> bool:
    loc = data.locations()[name]
    s = world.setting
    for region in ("Faron", "Eldin", "Lanayru"):
        if loc.original_item == f"{region} Twilight Tear" and s(f"{region} Twilight Cleared") == "On":
            return True
    ilia = world.setting_index("Ilia Memory Quest")
    idx = data.settings()["Ilia Memory Quest"].index_of
    return ((ilia >= idx("Letter") and name == "Renados Letter")
            or (ilia >= idx("Invoice") and name == "Telma Invoice")
            or (ilia >= idx("Statue") and name == "Wooden Statue")
            or (ilia >= idx("Charm") and name == "Ilia Charm"))


def is_vanilla_location(world: "TPWorld", loc: data.LocationData) -> str | None:
    """World::PlaceVanillaItems. Returns the item placed, or None when the location is shuffled."""
    s = world.setting
    items = data.items()
    orig = items[loc.original_item]
    oname = orig.name
    vanilla = (
        (s("Small Keys") == "Vanilla" and (bool(orig.small_key_of) or "Ordon Pumpkin" in oname or "Ordon Cheese" in oname))
        or (s("Big Keys") == "Vanilla" and bool(orig.big_key_of)
            and (oname != "Hyrule Castle Big Key" or s("Hyrule Castle Big Key Requirements") == "None"))
        or (s("Maps and Compasses") == "Vanilla" and bool(orig.map_of or orig.compass_of))
        or (oname == "Hyrule Castle Big Key" and s("Hyrule Castle Big Key Requirements") != "None")
        or (oname == "Poe Soul" and (s("Poe Souls") == "Vanilla"
                                     or (s("Poe Souls") == "Dungeon" and loc.has("Overworld"))
                                     or (s("Poe Souls") == "Overworld" and loc.has("Dungeon"))))
        or (s("Golden Bugs") == "Off" and loc.has("Golden Bug"))
        or (s("Sky Characters") == "Off" and loc.has("Sky Character"))
        or (s("Gifts From NPCs") == "Off" and loc.has("Npc"))
        or (s("Shop Items") == "Off" and loc.has("Shop"))
        or (s("Hidden Skills") == "Off" and loc.has("Golden Wolf"))
        or (s("Hidden Rupees") == "Off" and loc.has("Rupee - Hidden"))
        or (s("Freestanding Rupees") == "Off" and loc.has("Rupee - Freestanding"))
        or loc.has("Warp Portal")
        or any(n in loc.name for n in ALWAYS_VANILLA_NAMES)
    )
    if not vanilla:
        return None
    if orig.is_bottle:
        return "Empty Bottle"
    if orig.is_stamp:
        return "Purple Rupee"
    return oname


def removed_by_nonprogress(world: "TPWorld", loc: data.LocationData) -> bool:
    """World::SetNonProgressLocations: vanilla items that conflict with other settings get emptied."""
    s = world.setting
    orig = data.items()[loc.original_item]
    if s("Gifts From NPCs") == "Off" and loc.has("Npc") and (
            (s("Small Keys") == "Keysy" and orig.small_key_of)
            or (s("Big Keys") == "Keysy" and orig.big_key_of)
            or (s("Maps and Compasses") == "Start With" and (orig.map_of or orig.compass_of))):
        return True
    # "Shop Items Off and starting with the shop item": AP start inventory is separate from
    # the randomizer's starting pool, so only intrinsic starting items can trigger this.
    if s("Shop Items") == "Off" and loc.has("Shop") and orig.name in starting_items(world):
        return True
    return False
