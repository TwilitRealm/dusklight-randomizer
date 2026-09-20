"""Twilight Princess (Dusklight) — Archipelago world driven by the official randomizer's data.

Logic, locations and items are read from the same YAML files the Dusklight randomizer mod
generates seeds from. The mod rebuilds the full seed in-game from slot_data (settings +
every placement), so everything the official randomizer patches (stage edits, text, flags,
progressive items) comes out identical to a normal randomizer seed.
"""
from __future__ import annotations

import logging
from collections import Counter
from typing import Any, ClassVar

from BaseClasses import (CollectionState, Entrance, Item, ItemClassification, Location,
                         LocationProgressType, MultiWorld, Region, Tutorial)
from worlds.AutoWorld import WebWorld, World
from worlds.LauncherComponents import Component, Type, components

from . import data, logic
from .logic import FORM_NAMES, FORM_TIMES, HUMAN_DAY, HUMAN_NIGHT, TWILIGHT, WOLF_DAY, WOLF_NIGHT
from .options import TPOptions, resolve_settings
from .pools import (JUNK_POOL, build_item_pool, is_vanilla_location, removed_by_nonprogress,
                    should_remove_location, starting_items)

GAME = "Twilight Princess (Dusklight)"
AP_ITEM_NAME = "Archipelago Item"
SLOT_DATA_VERSION = 1

_ITEMS = data.items()
_LOCATIONS = data.locations()

# Items that exist only for logic (ids above the u8 item space, e.g. "Game Beatable").
_GIVEABLE = {name: it for name, it in _ITEMS.items() if it.id <= 0xFF and name != AP_ITEM_NAME}
_REAL_LOCATIONS = {name: loc for name, loc in _LOCATIONS.items()
                   if not loc.has("Non-Item Location") and not loc.has("Placeholder")}

_SKIP_BALANCING = {"Poe Soul", "Piece of Heart", "Heart Container"}
_GOAL_ITEMS = ("Progressive Mirror Shard", "Progressive Fused Shadow")


class TPItem(Item):
    game = GAME


class TPLocation(Location):
    game = GAME

    def __init__(self, player: int, name: str, address: int | None, parent: Region,
                 loc_data: data.LocationData | None = None):
        super().__init__(player, name, address, parent)
        self.loc_data = loc_data


class TPWeb(WebWorld):
    theme = "grassFlowers"
    tutorials = [Tutorial(
        "Multiworld Setup Guide",
        "Install the Dusklight Archipelago mod and connect from the in-game Archipelago mode.",
        "English", "setup_en.md", "setup/en", ["noahsmaximum"])]


def _launch_client(*args: str) -> None:
    logging.info("Twilight Princess (Dusklight) connects from inside the game: "
                 "select the Archipelago game mode in Dusklight.")


components.append(Component("Twilight Princess (Dusklight)", func=_launch_client,
                            component_type=Type.CLIENT))


class TPWorld(World):
    """Twilight Princess, played natively on PC through Dusklight."""

    game = GAME
    web = TPWeb()
    options_dataclass = TPOptions
    options: TPOptions  # type: ignore[assignment]
    topology_present = True

    item_name_to_id: ClassVar[dict[str, int]] = {
        name: data.ITEM_ID_BASE + it.id for name, it in _GIVEABLE.items()}
    location_name_to_id: ClassVar[dict[str, int]] = {
        name: loc.ap_id for name, loc in _REAL_LOCATIONS.items()}
    item_name_groups = {
        "Golden Bugs": {n for n, it in _ITEMS.items() if it.is_golden_bug},
        "Small Keys": {n for n, it in _ITEMS.items() if it.small_key_of},
        "Big Keys": {n for n, it in _ITEMS.items() if it.big_key_of},
        "Portals": {n for n in _ITEMS if n.endswith(" Portal")},
    }

    settings_map: dict[str, str]
    progression_items: set[str]

    # ------------------------------------------------------------------------------------
    def generate_early(self) -> None:
        s = resolve_settings(self.options)
        # World::ResolveConflictingSettings
        if (s["Bonks Do Damage"] == "On" and s["Logic Damage Multiplier"] == "OHKO"
                and (s["Eldin Twilight Cleared"] == "Off" or s["Lanayru Twilight Cleared"] == "Off")):
            s["Bonks Do Damage"] = "Off"
        if s["Starting Form"] == "Wolf" and s["Skip Prologue"] == "Off":
            s["Skip Prologue"] = "On"
        self.settings_map = s
        self._event_names: dict[str, str] = {}

    def setting(self, name: str) -> str:
        return self.settings_map[name]

    def setting_index(self, name: str) -> int:
        return data.settings()[name].index_of(self.settings_map[name])

    def _event_item(self, event: str) -> str:
        name = self._event_names.get(event)
        if name is None:
            name = event if event not in _ITEMS else f"Event: {event}"
            self._event_names[event] = name
        return name

    # ------------------------------------------------------------------------------------
    def create_regions(self) -> None:
        mw, p = self.multiworld, self.player
        areas = data.areas()
        parser = logic.Parser(self.setting)
        golden_bugs = tuple(n for n, it in _ITEMS.items() if it.is_golden_bug)
        comp = logic.Compiler(parser, p, self._event_item, golden_bugs)
        self._compiler = comp

        present_locations = {n for n in _LOCATIONS if not should_remove_location(self, n)}

        # Parse everything up front so referenced events are known before creating them.
        for area in areas.values():
            for req in (*area.events.values(), *area.locations.values(), *area.exits.values()):
                parser.parse(req)
        for macro in data.macros():
            parser.parse_macro(macro)
        referenced_events = set(parser.referenced_events) | set(logic.DUNGEON_COMPLETION_EVENTS)

        menu = Region("Menu", p, mw)
        mw.regions.append(menu)

        def twilight_active(a: data.AreaData) -> bool:
            return bool(a.twilight) and self.setting(f"{a.twilight} Twilight Cleared") == "Off"

        hubs: dict[str, Region] = {}
        copies: dict[str, dict[int, Region]] = {}
        for a in areas.values():
            hub = Region(a.name, p, mw)
            hubs[a.name] = hub
            mw.regions.append(hub)
            cs: dict[int, Region] = {}
            forms = FORM_TIMES + ((TWILIGHT,) if twilight_active(a) else ())
            for ft in forms:
                r = Region(f"{a.name} ({FORM_NAMES[ft]})", p, mw)
                mw.regions.append(r)
                r.connect(hub, f"{r.name} -> hub")
                cs[ft] = r
            copies[a.name] = cs

        for ft in FORM_TIMES:
            menu.connect(copies["Root"][ft], f"Menu -> Root ({FORM_NAMES[ft]})")

        cleared_cache: dict[str, logic.Rule] = {}

        def cleared(a: data.AreaData) -> logic.Rule:
            if not twilight_active(a):
                return True
            rule = cleared_cache.get(a.twilight)
            if rule is None:
                rule = comp.compile_str(f"Can_Complete_{a.twilight.replace(' ', '_')}_Twilight", logic.ALL)
                cleared_cache[a.twilight] = rule
            return rule

        def area_forms(a: data.AreaData) -> logic.AreaForms:
            cs = copies[a.name]

            def reach(*fts: int) -> logic.Rule:
                regs = tuple(cs[ft] for ft in fts if ft in cs)
                if not regs:
                    return False
                if len(regs) == 1:
                    r0 = regs[0]
                    return lambda state: r0.can_reach(state)
                r0, r1 = regs
                return lambda state: r0.can_reach(state) or r1.can_reach(state)

            return logic.AreaForms(
                human=reach(HUMAN_DAY, HUMAN_NIGHT), wolf=reach(WOLF_DAY, WOLF_NIGHT),
                day=reach(HUMAN_DAY, WOLF_DAY), night=reach(HUMAN_NIGHT, WOLF_NIGHT),
                twilight=reach(TWILIGHT))

        def connect(src: Region, dst: Region, rule: logic.Rule, name: str) -> None:
            if rule is False:
                return
            e = src.connect(dst, name)
            if rule is not True:
                e.access_rule = rule

        shadow_crystal = "Shadow Crystal"
        comp.referenced_items.add(shadow_crystal)
        for a in areas.values():
            cs = copies[a.name]
            clr = cleared(a)
            can_transform = a.can_transform == "Always" or (
                a.can_transform == "If Transform Anywhere" and self.setting("Logic Transform Anywhere") == "On")
            # Search::ExpandFormTimes as edges between this area's form-time copies
            if a.can_change_time:
                for x, y in ((HUMAN_DAY, HUMAN_NIGHT), (WOLF_DAY, WOLF_NIGHT)):
                    connect(cs[x], cs[y], clr, f"{a.name}: {FORM_NAMES[x]} -> {FORM_NAMES[y]}")
                    connect(cs[y], cs[x], clr, f"{a.name}: {FORM_NAMES[y]} -> {FORM_NAMES[x]}")
            if can_transform:
                sc = comp.all_of([lambda state: state.has(shadow_crystal, self.player), clr])
                for x, y in ((HUMAN_DAY, WOLF_DAY), (HUMAN_NIGHT, WOLF_NIGHT)):
                    connect(cs[x], cs[y], sc, f"{a.name}: {FORM_NAMES[x]} -> {FORM_NAMES[y]}")
                    connect(cs[y], cs[x], sc, f"{a.name}: {FORM_NAMES[y]} -> {FORM_NAMES[x]}")

            for dest_name, req in a.exits.items():
                dest = areas[dest_name]
                dcs = copies[dest_name]
                dclr = cleared(dest)
                node = parser.parse(req)
                for ft in FORM_TIMES:
                    rule = comp.all_of([dclr, comp.compile(node, ft)])
                    connect(cs[ft], dcs[ft], rule, f"{a.name} -> {dest_name} ({FORM_NAMES[ft]})")
                if TWILIGHT in dcs:
                    # EvaluateExitRequirement: while the destination's twilight is uncleared,
                    # success at *any* of the parent's form-times (or at Twilight itself,
                    # which is always added) spreads only the Twilight bit.
                    for ft in FORM_TIMES:
                        connect(cs[ft], dcs[TWILIGHT], comp.compile(node, ft),
                                f"{a.name} -> {dest_name} ({FORM_NAMES[ft]} into Twilight)")
                    connect(hubs[a.name], dcs[TWILIGHT], comp.compile(node, TWILIGHT),
                            f"{a.name} -> {dest_name} (Twilight)")

        # Location and event access lists (a location/event may be reachable from several areas)
        loc_access: dict[str, list[tuple[data.AreaData, str]]] = {}
        event_access: dict[str, list[tuple[data.AreaData, str]]] = {}
        for a in areas.values():
            evs = dict(a.events)
            evs.setdefault(f"Can Access {a.name}", "Nothing")
            if a.can_warp:
                evs.setdefault("Can Warp", "Nothing")
            if a.map_sector:
                evs.setdefault(f"{a.map_sector} Map Sector", "Nothing")
            for ev, req in evs.items():
                if ev in referenced_events:
                    event_access.setdefault(ev, []).append((a, req))
            for loc_name, req in a.locations.items():
                if loc_name not in present_locations:
                    continue
                if a.twilight and not _LOCATIONS[loc_name].original_item.endswith("Twilight Tear"):
                    req = f"Not_Twilight and ({req})"
                loc_access.setdefault(loc_name, []).append((a, req))

        forms_cache: dict[str, logic.AreaForms] = {}

        def access_rule(accesses: list[tuple[data.AreaData, str]]) -> tuple[Region, logic.Rule]:
            """Where to put the location, and the rule that guards it.

            A location or event can be defined in several areas (Coro Lantern exists both in
            Prologue Woods and in Faron Woods). AP only considers a location reachable when its
            own region is, so anything with more than one access lives in Menu and checks each
            area's reachability in its rule instead.
            """
            multi = len(accesses) > 1
            home = menu if multi else hubs[accesses[0][0].name]
            parts: list[logic.Rule] = []
            for a, req in accesses:
                forms = forms_cache.get(a.name)
                if forms is None:
                    forms = forms_cache[a.name] = area_forms(a)
                r = comp.compile_str(req, forms)
                if multi and r is not False:
                    hub = hubs[a.name]
                    r = comp.all_of([lambda state, h=hub: h.can_reach(state), r])
                parts.append(r)
            return home, comp.any_of(parts)

        # Events
        for ev, accesses in event_access.items():
            for a, req in accesses:
                region, rule = access_rule([(a, req)])
                if rule is False:
                    continue
                loc = TPLocation(p, f"{ev} @ {a.name}", None, region)
                loc.place_locked_item(TPItem(self._event_item(ev), ItemClassification.progression, None, p))
                loc.show_in_spoiler = False
                if rule is not True:
                    loc.access_rule = rule
                region.locations.append(loc)
        missing = referenced_events - set(event_access) - set(logic.DUNGEON_COMPLETION_EVENTS)
        if missing:
            raise logic.LogicError(f"events used but never defined: {sorted(missing)}")

        # Item locations: real AP locations, or vanilla-locked ones as events carrying the real item
        self._vanilla_locked: dict[str, str] = {}
        self._excluded_nonprogress: set[str] = set()
        self._real_location_names: list[str] = []
        pending_locked: list[tuple[TPLocation, str]] = []
        for loc_name in _LOCATIONS:
            ld = _LOCATIONS[loc_name]
            if loc_name not in present_locations or ld.has("Non-Item Location"):
                continue
            accesses = loc_access.get(loc_name)
            if not accesses:
                continue
            region, rule = access_rule(accesses)
            vanilla = is_vanilla_location(self, ld)
            if vanilla and removed_by_nonprogress(self, ld):
                vanilla = None
                self._excluded_nonprogress.add(loc_name)
            if vanilla is not None:
                loc = TPLocation(p, loc_name, None, region, ld)
                pending_locked.append((loc, vanilla))
                self._vanilla_locked[loc_name] = vanilla
            else:
                if loc_name not in _REAL_LOCATIONS:
                    continue
                loc = TPLocation(p, loc_name, ld.ap_id, region, ld)
                self._real_location_names.append(loc_name)
                if loc_name in self._excluded_nonprogress:
                    loc.progress_type = LocationProgressType.EXCLUDED
            if rule is not True:
                loc.access_rule = logic.as_callable(rule)
            region.locations.append(loc)

        # Randomizer-intrinsic starting items (portals, start-with maps) as logic-only events
        self._starting = starting_items(self)
        for name, count in self._starting.items():
            for i in range(count):
                loc = TPLocation(p, f"Start: {name} #{i + 1}", None, menu)
                loc.show_in_spoiler = False
                menu.locations.append(loc)
                pending_locked.append((loc, name))

        # Classification needs every compiled rule's referenced items, so lock items last.
        self.progression_items = set(comp.referenced_items)
        for loc, name in pending_locked:
            loc.place_locked_item(self._make_item(name, event=True))

        mw.completion_condition[p] = lambda state: state.has("Game Beatable", p)

    # ------------------------------------------------------------------------------------
    def _classification(self, name: str) -> ItemClassification:
        it = _ITEMS[name]
        if it.game_winning:
            return ItemClassification.progression
        if name == "Foolish Item":
            return ItemClassification.trap
        if name in self.progression_items:
            if name in _SKIP_BALANCING or it.is_golden_bug:
                return ItemClassification.progression_skip_balancing
            return ItemClassification.progression
        if it.importance in ("Major", "Minor"):
            return ItemClassification.useful
        return ItemClassification.filler

    def _make_item(self, name: str, event: bool = False) -> TPItem:
        code = None if event else self.item_name_to_id[name]
        cls = self._classification(name)
        if event and not cls & ItemClassification.progression:
            cls = ItemClassification.filler
        return TPItem(name, cls, code, self.player)

    def create_item(self, name: str) -> TPItem:
        return self._make_item(name)

    def get_filler_item_name(self) -> str:
        return self.random.choice(sorted(JUNK_POOL))

    def create_items(self) -> None:
        pool = build_item_pool(self)
        # PlaceVanillaItems: each vanilla-locked location consumes one copy from the pool
        for item_name in self._vanilla_locked.values():
            if pool.get(item_name, 0) > 0:
                pool[item_name] -= 1
        for name, count in self._starting.items():
            pool[name] = max(0, pool.get(name, 0) - count)

        items: list[str] = [n for n, c in pool.items() for _ in range(c) if n in _GIVEABLE]
        n_locations = len(self._real_location_names)

        # World::SanitizeItemPool: junk up to the location count
        junk = Counter(JUNK_POOL)
        freq = self.setting("Trap Item Frequency")
        if freq == "Few":
            junk["Foolish Item"] = 6
        elif freq == "Many":
            junk["Foolish Item"] = 27
        elif freq == "Mayhem":
            junk["Foolish Item"] = 64
        elif freq == "Nightmare":
            junk = Counter({"Foolish Item": 1})
        junk_list = [n for n, c in sorted(junk.items()) for _ in range(c)]
        self.random.shuffle(junk_list)
        junk_copy = list(junk_list)
        while len(items) < n_locations:
            items.append(junk_list.pop() if junk_list else self.random.choice(junk_copy))
        if len(items) > n_locations:
            # Too few locations for the pool (small location settings): drop junk first.
            surplus = len(items) - n_locations
            items.sort(key=lambda n: self._classification(n) != ItemClassification.filler)
            dropped, items = items[:surplus], items[surplus:]
            if any(self._classification(n) != ItemClassification.filler for n in dropped):
                logging.warning(f"{self.player_name}: item pool exceeds locations; dropped "
                                f"{len(dropped)} items including non-filler.")
        self.multiworld.itempool += [self.create_item(n) for n in items]

    # ------------------------------------------------------------------------------------
    def set_rules(self) -> None:
        # Small keys on bosses (World::SetForbiddenItems)
        if self.setting("Small Keys on Bosses") == "Off":
            keys = {n for n, it in _ITEMS.items() if it.small_key_of} | {
                "Ordon Pumpkin", "Ordon Cheese", "North Faron Woods Gate Key", "Faron Woods Coro Key",
                "Gate Keys", "Gerudo Desert Bulblin Camp Key"}
            for name in self._real_location_names:
                if "Heart Container" in name or "Dungeon Reward" in name:
                    loc = self.get_location(name)
                    loc.item_rule = lambda item, k=keys: not (item.player == self.player and item.name in k)

    def generate_basic(self) -> None:
        # Events whose area is unreachable even with every item (e.g. gated off by settings)
        # can never be collected; drop them so AP's full-accessibility check doesn't demand them.
        state = CollectionState(self.multiworld)
        for item in self.multiworld.itempool:
            if item.player == self.player:
                state.collect(item, True)
        state.sweep_for_advancements(locations=self.get_locations())
        for region in self.get_regions():
            dead = [l for l in region.locations if l.address is None and not l.can_reach(state)
                    and not (l.item and l.item.name == "Game Beatable")]
            for l in dead:
                region.locations.remove(l)

    def pre_fill(self) -> None:
        """Place restricted items (dungeon rewards, own/any-dungeon/overworld keys) like the rando's fill."""
        from Fill import fill_restrictive  # imported here: Fill imports worlds at module level
        mw, p = self.multiworld, self.player
        groups: list[tuple[list[str], list[TPLocation]]] = []
        unfilled = [self.get_location(n) for n in self._real_location_names
                    if self.get_location(n).item is None]

        def pop_items(pred) -> list[Item]:
            taken = [it for it in mw.itempool if it.player == p and pred(it.name)]
            for it in taken:
                mw.itempool.remove(it)
            return taken

        if self.setting("Dungeon Rewards Can Be Anywhere") == "Off":
            goal_locs = [l for l in unfilled if l.loc_data and l.loc_data.goal]
            groups.append((pop_items(lambda n: n in _GOAL_ITEMS), goal_locs))

        category_setting = {"small": "Small Keys", "big": "Big Keys", "map": "Maps and Compasses"}

        def item_kind(name: str) -> str | None:
            it = _ITEMS.get(name)
            if it is None:
                return None
            if it.small_key_of:
                return "small"
            if it.big_key_of:
                return "big"
            if it.map_of or it.compass_of:
                return "map"
            return None

        for dungeon in data.DUNGEONS:
            for kind, setting in category_setting.items():
                if self.setting(setting) != "Own Dungeon":
                    continue
                locs = [l for l in unfilled if l.loc_data and l.loc_data.has(dungeon) and l.loc_data.has("Dungeon")]
                its = pop_items(lambda n, k=kind: item_kind(n) == k and _ITEMS[n].dungeon == dungeon)
                if its:
                    groups.append((its, locs))
        for kind, setting in category_setting.items():
            mode = self.setting(setting)
            if mode == "Any Dungeon":
                locs = [l for l in unfilled if l.loc_data and l.loc_data.has("Dungeon")]
            elif mode == "Overworld":
                locs = [l for l in unfilled if l.loc_data and l.loc_data.has("Overworld")]
            else:
                continue
            its = pop_items(lambda n, k=kind: item_kind(n) == k)
            if its:
                groups.append((its, locs))

        for its, locs in groups:
            if not its:
                continue
            state = CollectionState(mw)
            for item in mw.itempool:
                if item.player == p:
                    state.collect(item, True)
            for other_its, _ in groups:
                if other_its is not its:
                    for item in other_its:
                        if item.location is None:
                            state.collect(item, True)
            state.sweep_for_advancements(locations=self.get_locations())
            free = [l for l in locs if l.item is None]
            self.random.shuffle(free)
            fill_restrictive(mw, state, free, its, single_player_placement=True, lock=True,
                             allow_excluded=True, name=f"TP Dusklight pre-fill ({self.player_name})")
            if its:
                raise RuntimeError(f"{self.player_name}: could not place restricted items {[i.name for i in its]}")

    # ------------------------------------------------------------------------------------
    def fill_slot_data(self) -> dict[str, Any]:
        placements: dict[str, Any] = {}
        location_ids: dict[str, int] = {}
        for name in self._real_location_names:
            loc = self.get_location(name)
            location_ids[name] = loc.address
            item = loc.item
            if item is None:
                continue
            if item.player == self.player and item.game == GAME:
                placements[name] = item.name
            else:
                placements[name] = {
                    "item": AP_ITEM_NAME,
                    "name": item.name,
                    "player": self.multiworld.player_name[item.player],
                    "flags": int(item.classification),
                }
        return {
            "version": SLOT_DATA_VERSION,
            "data_version": data.data_version(),
            "seed": self.multiworld.seed_name,
            "settings": self.settings_map,
            "placements": placements,
            "location_ids": location_ids,
            "item_id_base": data.ITEM_ID_BASE,
        }

    def write_spoiler_header(self, spoiler_handle) -> None:
        spoiler_handle.write(f"Vanilla-locked checks ({self.player_name}): {len(self._vanilla_locked)}\n")
