# Twilight Princess (Dusklight) Setup Guide

## Required software

- [Dusklight](https://github.com/TwilitRealm/dusklight) v2.0.0 or newer, with a Twilight Princess disc image.
- The `archipelago.dusk` mod from [dusklight-archipelago](https://github.com/noahsmaximum/dusklight-archipelago/releases).
- The `tp_dusklight.apworld` from the same release.

## Installation

1. Copy `archipelago.dusk` into your Dusklight mods folder:
   - Windows: `%APPDATA%\TwilitRealm\Dusklight\mods`
   - Linux: `~/.local/share/TwilitRealm/Dusklight/mods`
   - macOS: `~/Library/Application Support/TwilitRealm/Dusklight/mods`
2. Copy `tp_dusklight.apworld` into your Archipelago install's `custom_worlds` folder.

## Configuring your YAML

Generate a template from the Archipelago Launcher ("Generate Template Options"), or use the
YAML from the release. The options match the settings in the in-game randomizer menus.

There is no separate client to run and no ROM to patch: the game builds the seed itself when
it connects.

## Joining a multiworld

1. Start Dusklight and use the arrows on the Play button to select **Archipelago**.
2. Create a new file. In the connection window enter the server address, your slot name and the
   room password if there is one, then press **Connect and start**.
3. The seed is built from the server, and you continue to name entry.

Enter the address exactly as Archipelago gives it to you (for example `archipelago.gg:12345`).
Encrypted and unencrypted rooms both work; the mod picks whichever the room uses.

Loading the save later reconnects on its own. The **Archipelago** tab in the menu bar (F1)
shows the connection status and lets you change the server if the room moved.
