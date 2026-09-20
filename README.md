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

Both `ws://` and `wss://` work. The mod speaks WebSocket itself over a plain TCP socket, and
wraps that in its own TLS for `wss://`, so nothing depends on the host's WebSocket support.

Type the room address the way Archipelago gives it to you (`archipelago.gg:12345`). Rooms are
served either encrypted or plain, never both, so the mod tries the likely one first — TLS for
a remote server, plain for `localhost` — and falls back to the other if that is refused. You
can force one by typing the scheme yourself: `wss://archipelago.gg:12345`.

Server certificates are checked against a list of root authorities built into the mod, so a
room with an expired, self-signed or mismatched certificate is refused rather than silently
trusted. If you run your own server with a self-signed certificate, connect to it over plain
`ws://` instead.

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

`tools/tls_test.cpp` builds a `tls_test` executable that drives the mod's TLS client
(`src/ap/tls.cpp`) over ordinary sockets against real servers, checking both that valid
certificates are accepted and that expired, self-signed, untrusted and mismatched ones are
refused. Run it with no arguments for the default suite.

The trusted roots in `src/ap/ca_bundle.pem` come from
[curl.se/docs/caextract.html](https://curl.se/docs/caextract.html) (Mozilla's list). Replace
that file to refresh them; nothing else needs to change.

## Credits

The randomizer, its logic data and its generator are by [Twilit Realm](https://github.com/TwilitRealm).
This fork adds the Archipelago game mode, the network client and the apworld.
