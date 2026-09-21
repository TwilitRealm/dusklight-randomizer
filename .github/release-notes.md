## Install

1. **Mod** — put `archipelago.dusk` in your Dusklight mods folder:
   - Windows: `%APPDATA%\TwilitRealm\Dusklight\mods`
   - Linux: `~/.local/share/TwilitRealm/Dusklight/mods`
   - macOS: `~/Library/Application Support/TwilitRealm/Dusklight/mods`
2. **Apworld** — put `tp_dusklight.apworld` in your Archipelago `custom_worlds` folder.
3. **YAML** — grab a preset below, or generate a template from the Archipelago Launcher. Set `name:` to your slot name.

Then in Dusklight: use the arrows on the Play button to pick **Archipelago**, start a **new file**, and enter your server address, slot name and password. The seed builds from the server and you play.

Needs Dusklight 2.0.1 or newer.

## What's new in 0.2.0

- **Encrypted rooms work.** The mod now does TLS itself, so `archipelago.gg` rooms connect and send checks. Enter the address as Archipelago gives it to you (`archipelago.gg:12345`); the mod works out whether the room is encrypted.
- **Certificates are checked**, so a room with a bad certificate is refused instead of silently trusted. Self-signed server? Use `ws://`.
- **A hostile server can't crash the game** through item text or by flooding the connection.
- **Randomizer fixes from upstream**, including a logic change: reaching Eldin Field from north Eldin now needs a way to smash.
- Built against **Dusklight 2.0.1**, which fixes crashes in mods that replace dialogue.

## Updating

Update the mod and the apworld **together** — the logic data changed and the mod can't detect a mismatch. Finish any multiworld already in progress on the versions you started it with.

Not yet tested in a live hosted room; a locally hosted `ws://` server is the path that's been played end to end. Report anything you hit.
