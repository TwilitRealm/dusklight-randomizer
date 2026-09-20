# Dusklight Archipelago

[Archipelago](https://archipelago.gg) multiworld support for [Dusklight](https://github.com/TwilitRealm/dusklight),
built on top of the [official Dusklight randomizer](https://github.com/TwilitRealm/dusklight-randomizer).

The mod connects to an Archipelago server from inside the game and rebuilds the seed with the
randomizer's own generator, so a multiworld seed plays exactly like a normal randomizer seed:
same logic, same stage edits, same text, same progressive items. There is no separate client to
run and nothing to patch.

## Playing

You need three things:

1. **The mod.** Copy `archipelago.dusk` into your Dusklight mods folder:
   - Windows: `%APPDATA%\TwilitRealm\Dusklight\mods`
   - Linux: `~/.local/share/TwilitRealm/Dusklight/mods`
   - macOS: `~/Library/Application Support/TwilitRealm/Dusklight/mods`
2. **The apworld.** Put `tp_dusklight.apworld` into your Archipelago install's `custom_worlds`
   folder. Whoever generates the multiworld needs it; players who only play need it for the
   tracker and text client.
3. **A YAML.** Generate a template from the Archipelago Launcher, or copy
   `Twilight Princess (Dusklight).yaml` from the release and edit it.

Then:

1. Start Dusklight. On the Play button, use the arrows to pick **Archipelago**, and start it.
2. Create a **new file**. A connection window opens: enter the server (`archipelago.gg:12345`
   or `localhost:38281`), your slot name from the YAML, and the room password if there is one.
3. Press **Connect and start**. The seed is built from the server's data, which takes a few
   seconds, and then you continue to name entry as usual.
4. Play. Checks are sent as you collect them, and items other players find for you arrive
   automatically. Items belonging to other worlds appear as a Sol and say who they belong to.

Loading an existing Archipelago save reconnects by itself. If the room moved to a different
port, open the **Archipelago** tab in the menu bar (F1), change the server there, and press
Reconnect. That tab also shows connection status, items received and checks sent.

Anything you collect while disconnected is sent the next time you connect, and items you were
given while away arrive when you load the save.

### Servers and encryption

Plain `ws://` servers — including Archipelago's hosted rooms and any server you run yourself —
use this mod's own WebSocket client and work fully. `wss://` (TLS) currently falls back to
Dusklight's built-in WebSocket support, which only delivers the first message a client sends,
so checks would never reach the server. Use the plain address for now.

## Presets

`presets/` holds four ready-made YAMLs, verified to generate and to rebuild in-game:

| Preset | Checks | What it is |
| --- | --- | --- |
| Easy | ~320 | Prologue, Midna's Desperate Hour and all three twilights done; dungeon items stay in their dungeon; only chests and freestanding items shuffled; plentiful pool, no traps, castle open. |
| Medium | ~455 | Prologue skipped, twilights mostly intact, keys move between dungeons, golden bugs, NPC gifts and hidden skills shuffled, a few traps, castle wants four dungeons. |
| Hard | ~570 | Nothing skipped, everything shuffled including shops, sky characters and every poe, keys anywhere, many traps, double damage, castle wants seven dungeons. |
| Extreme | ~570 | Hard plus a minimal pool, one-hit kills, bonks that hurt, traps everywhere, and a castle that wants all eight dungeons and all 60 poe souls. |

Copy one into your Archipelago `Players` folder and set `name:` to your slot name.

## Options

The YAML options are generated from the randomizer's own settings, so they match the names in
the in-game randomizer menus. A few are fixed by this world:

- **Entrance randomization** and **randomized starting spawn** are off. The in-game generator
  only sees your own world, so it can't place entrances consistently with the multiworld yet.
- **In-game hints** (hint signs, Midna hints) are off, because the randomizer's hint generator
  can't see other players' worlds. Use Archipelago's own hint system instead.
- **Unrequired dungeons are barren** is off, and logic is always "all locations reachable".

## Building

```sh
git clone https://github.com/noahsmaximum/dusklight-archipelago
cd dusklight-archipelago
cmake -B build
cmake --build build --parallel
```

`build/mods/archipelago.dusk` is the mod. Build the apworld with:

```sh
python tools/build_apworld.py          # writes build/tp_dusklight.apworld
python tools/build_apworld.py --install <Archipelago>/custom_worlds
```

The apworld vendors the randomizer's own YAML data (`generator/data`) and implements the same
logic in Python, so its locations, items and rules stay in step with the mod. Re-run
`tools/build_apworld.py` after changing anything under `generator/data`.

`tools/ap_gen_test.cpp` builds an `ap_gen_test` executable that rebuilds a seed from a saved
`slot_data.json` exactly like the mod does, which is the quickest way to check generation
changes without launching the game.

## Credits

The randomizer, its logic data and its generator are by [Twilit Realm](https://github.com/TwilitRealm).
This fork adds the Archipelago game mode, the network client and the apworld.
