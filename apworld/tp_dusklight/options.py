"""AP options generated from the randomizer's own settings_list.yaml.

Every exposed setting keeps the randomizer's exact name and option labels on the wire
(slot_data["settings"]), so the in-game generator reproduces the same world.
"""
from __future__ import annotations

import re
from dataclasses import make_dataclass
from typing import Any

from Options import (Choice, DefaultOnToggle, PerGameCommonOptions, Range, StartInventoryPool,
                     Toggle)

from . import data

# Settings the AP world pins instead of exposing, with the value it pins them to.
# Entrance shuffle and hints are decided by the in-game generator from local data only,
# which can't see other worlds, so they stay off; logic rules are always "all reachable".
FORCED: dict[str, str] = {
    "Logic Rules": "All Locations Reachable",
    "Unrequired Dungeons Are Barren": "Off",
    "Randomize Starting Spawn": "Off",
    "Randomize Dungeon Entrances": "Off",
    "Randomize Boss Entrances": "Off",
    "Randomize Grotto Entrances": "Off",
    "Randomize Cave Entrances": "Off",
    "Randomize Interior Entrances": "Off",
    "Randomize Overworld Entrances": "Off",
    "Decouple Double Door Entrances": "Off",
    "Decouple Entrances": "Off",
    "Number of Path Hints": "0",
    "Path Hints on Midna": "Off",
    "Path Hints on Hint Signs": "Off",
    "Number of Barren Hints": "0",
    "Barren Hints on Midna": "Off",
    "Barren Hints on Hint Signs": "Off",
    "Number of Item Hints": "0",
    "Item Hints on Midna": "Off",
    "Item Hints on Hint Signs": "Off",
    "Number of Location Hints": "0",
    "Location Hints on Midna": "Off",
    "Location Hints on Hint Signs": "Off",
    "Prioritize Remote Location Hints": "Off",
}


def option_key(setting_name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", setting_name.lower().replace("'", "")).strip("_")


def _option_attr(label: str) -> str:
    return "option_" + option_key(label)


def _make_option(info: data.SettingInfo) -> type:
    doc = f"{info.name} (randomizer setting)."
    if info.numeric:
        values = [int(o) for o in info.options]
        return type(option_key(info.name).title().replace("_", ""), (Range,), {
            "__doc__": doc,
            "display_name": info.name,
            "range_start": min(values),
            "range_end": max(values),
            "default": int(info.default),
        })
    if info.options == ["Off", "On"]:
        base = DefaultOnToggle if info.default == "On" else Toggle
        return type(option_key(info.name).title().replace("_", ""), (base,), {
            "__doc__": doc,
            "display_name": info.name,
        })
    attrs: dict[str, Any] = {"__doc__": doc, "display_name": info.name}
    for i, label in enumerate(info.options):
        attrs[_option_attr(label)] = i
    attrs["default"] = info.options.index(info.default)
    return type(option_key(info.name).title().replace("_", ""), (Choice,), attrs)


EXPOSED: dict[str, str] = {}  # option attribute -> setting name
_fields: list[tuple[str, type]] = []
for _info in data.settings().values():
    if _info.name in FORCED:
        continue
    _key = option_key(_info.name)
    EXPOSED[_key] = _info.name
    _fields.append((_key, _make_option(_info)))


_fields.append(("start_inventory_from_pool", StartInventoryPool))

TPOptions = make_dataclass("TPOptions", _fields, bases=(PerGameCommonOptions,))


def resolve_settings(options: Any) -> dict[str, str]:
    """Randomizer setting name -> option label, for every setting the generator knows."""
    out: dict[str, str] = {}
    for info in data.settings().values():
        if info.name in FORCED:
            out[info.name] = FORCED[info.name]
            continue
        opt = getattr(options, option_key(info.name))
        if info.numeric:
            out[info.name] = str(int(opt.value))
        elif info.options == ["Off", "On"]:
            out[info.name] = "On" if opt.value else "Off"
        else:
            out[info.name] = info.options[int(opt.value)]
    return out
