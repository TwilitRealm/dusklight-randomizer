"""Loads the official Dusklight randomizer's YAML data, vendored into this apworld.

The files under data/ are copied verbatim from the mod's generator/data directory by
tools/build_apworld.py, so the apworld's logic, item and location tables are always the
same ones the in-game generator uses.
"""
from __future__ import annotations

import functools
import pkgutil
import zlib
from dataclasses import dataclass, field
from typing import Any

import yaml

try:
    _Loader = yaml.CSafeLoader
except AttributeError:  # pragma: no cover - pure-python fallback
    _Loader = yaml.SafeLoader

WORLD_FILES = (
    "world/Root.yaml",
    "world/overworld/Ordona Province.yaml",
    "world/overworld/Faron Province.yaml",
    "world/overworld/Eldin Province.yaml",
    "world/overworld/Lanayru Province.yaml",
    "world/overworld/Snowpeak Province.yaml",
    "world/overworld/Gerudo Desert.yaml",
    "world/dungeons/Forest Temple.yaml",
    "world/dungeons/Goron Mines.yaml",
    "world/dungeons/Lakebed Temple.yaml",
    "world/dungeons/Arbiters Grounds.yaml",
    "world/dungeons/Snowpeak Ruins.yaml",
    "world/dungeons/Temple of Time.yaml",
    "world/dungeons/City in the Sky.yaml",
    "world/dungeons/Palace of Twilight.yaml",
    "world/dungeons/Hyrule Castle.yaml",
)

DUNGEONS = (
    "Forest Temple",
    "Goron Mines",
    "Lakebed Temple",
    "Arbiters Grounds",
    "Snowpeak Ruins",
    "Temple of Time",
    "City in the Sky",
    "Palace of Twilight",
    "Hyrule Castle",
)

# AP ids: base + game item id for items (item ids are unique u16s in items.yaml), and
# base + crc of the name for locations so ids survive upstream reordering.
ITEM_ID_BASE = 0x54500000
LOCATION_ID_BASE = 0x55000000


def _load(path: str) -> Any:
    raw = pkgutil.get_data(__name__, "data/" + path)
    if raw is None:
        raise FileNotFoundError(path)
    return yaml.load(raw.decode("utf-8"), Loader=_Loader)


@dataclass(frozen=True)
class ItemData:
    name: str
    id: int
    importance: str
    game_winning: bool
    small_key_of: str
    big_key_of: str
    compass_of: str
    map_of: str

    @property
    def dungeon(self) -> str:
        return self.small_key_of or self.big_key_of or self.compass_of or self.map_of

    @property
    def is_golden_bug(self) -> bool:
        return self.name.startswith("Male") or self.name.startswith("Female")

    @property
    def is_bottle(self) -> bool:
        return self.name.startswith("Bottle") or self.name == "Empty Bottle"

    @property
    def is_stamp(self) -> bool:
        return self.name.startswith("Stamp")


@dataclass(frozen=True)
class LocationData:
    name: str
    original_item: str
    categories: frozenset[str]
    goal: bool
    index: int

    @property
    def ap_id(self) -> int:
        return LOCATION_ID_BASE + (zlib.crc32(self.name.encode("utf-8")) & 0xFFFFFF)

    def has(self, *cats: str) -> bool:
        return all(c in self.categories for c in cats)


@dataclass
class SettingInfo:
    name: str
    default: str
    options: list[str]
    need_in_game: bool
    numeric: bool

    def index_of(self, option: str) -> int:
        return self.options.index(option)


@dataclass
class AreaData:
    name: str
    region: str
    twilight: str
    can_change_time: bool
    can_transform: str
    can_warp: bool
    map_sector: str
    events: dict[str, str] = field(default_factory=dict)
    locations: dict[str, str] = field(default_factory=dict)
    exits: dict[str, str] = field(default_factory=dict)


@functools.cache
def items() -> dict[str, ItemData]:
    out: dict[str, ItemData] = {}
    for node in _load("items.yaml"):
        out[node["Name"]] = ItemData(
            name=node["Name"],
            id=int(node["Id"]),
            importance=node["Importance"],
            game_winning=bool(node.get("Game Winning Item", False)),
            small_key_of=node.get("Dungeon Small Key", "") or "",
            big_key_of=node.get("Dungeon Big Key", "") or "",
            compass_of=node.get("Dungeon Compass", "") or "",
            map_of=node.get("Dungeon Map", "") or "",
        )
    return out


@functools.cache
def locations() -> dict[str, LocationData]:
    out: dict[str, LocationData] = {}
    for index, node in enumerate(_load("locations.yaml")):
        cats = set(c for c in (node.get("Categories") or []) if c is not None)
        meta = node.get("Metadata")
        if isinstance(meta, dict):
            cats.update(meta.keys())
        out[node["Name"]] = LocationData(
            name=node["Name"],
            original_item=node.get("Original Item", "Nothing"),
            categories=frozenset(cats),
            goal="Goal Name" in node,
            index=index,
        )
    ids = [loc.ap_id for loc in out.values()]
    if len(ids) != len(set(ids)):
        raise RuntimeError("tp_dusklight: location id collision; widen the id hash")
    return out


@functools.cache
def macros() -> dict[str, str]:
    return {str(k): str(v) for k, v in _load("macros.yaml").items()}


@functools.cache
def settings() -> dict[str, SettingInfo]:
    out: dict[str, SettingInfo] = {}
    for node in _load("settings_list.yaml"):
        options: list[str] = []
        numeric = False
        for opt in node["Options"]:
            label = str(next(iter(opt))) if isinstance(opt, dict) else str(opt)
            lo, sep, hi = label.partition("-")
            if sep and lo.isdigit() and hi.isdigit():
                numeric = True
                options.extend(str(n) for n in range(int(lo), int(hi) + 1))
            else:
                options.append(label)
        out[node["Name"]] = SettingInfo(
            name=node["Name"],
            default=str(node["Default Option"]),
            options=options,
            need_in_game=bool(node.get("Need In Game", False)),
            numeric=numeric,
        )
    return out


@functools.cache
def areas() -> dict[str, AreaData]:
    out: dict[str, AreaData] = {}
    for path in WORLD_FILES:
        for node in _load(path):
            name = node["Name"]
            out[name] = AreaData(
                name=name,
                region=node.get("Region", "") or "",
                twilight=node.get("Twilight", "") or "",
                can_change_time=bool(node.get("Can Change Time", False)),
                can_transform=node.get("Can Transform", "Always") or "Always",
                can_warp=bool(node.get("Can Warp", False)),
                map_sector=node.get("Map Sector", "") or "",
                events={str(k): str(v) for k, v in (node.get("Events") or {}).items()},
                locations={str(k): str(v) for k, v in (node.get("Locations") or {}).items()},
                exits={str(k): str(v) for k, v in (node.get("Exits") or {}).items()},
            )
    return out


def data_version() -> int:
    """Fingerprint of the vendored data, sent to the mod in slot_data.

    The mod does not check it yet: an old mod paired with a newer apworld will build a seed
    with its own older logic instead of refusing. Update both together.
    """
    crc = 0
    for path in ("items.yaml", "locations.yaml", "macros.yaml", "settings_list.yaml", *WORLD_FILES):
        crc = zlib.crc32((pkgutil.get_data(__name__, "data/" + path) or b"").replace(b"\r", b""), crc)
    return crc
