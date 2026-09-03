#pragma once

#include "log.hpp"

#ifndef RANDOMIZER_ONLY
#include "../../src/ui/rando_seed_generation.hpp"
#define UPDATE_PROGRESS_PERCENT(value) randomizer::ui::UpdateSeedGenProgressValue(value / 100.0f);
#define UPDATE_STATUS_MESSAGE(message) randomizer::ui::UpdateGenerationStatusMsg(message); \
                                       utility::platform::Log(message);
#else
#define UPDATE_PROGRESS_PERCENT(value)
#define UPDATE_STATUS_MESSAGE(message) utility::platform::Log(message);
#endif