#pragma once

// Archipelago game mode: connects to an AP server from inside the game, rebuilds the seed
// from slot data with the randomizer's own generator, then delivers received items and
// reports checks while the randomizer runtime plays the seed like any other.

#include "mods/svc/game_mode.h"

namespace ap {

GameModeDesc game_mode_desc();

// Randomizer game-mode lifecycle
ModResult activate();
void deactivate();
void tick();
// Every host frame, even while the game is paused behind a UI window (network, seed build).
void update();

// Replaces the randomizer's new-file seed gate: connect to the server, build the seed from
// slot data, then continue to name entry.
ModResult open_connect_gate(void* fileSelect);

// Called by the randomizer's procCoGetItem hook: lets AP items use custom get-item text.
void on_get_item_demo(void* link);

}  // namespace ap
