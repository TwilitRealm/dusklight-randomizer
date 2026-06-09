#include "rando_seed_generation.hpp"

#include "modal.hpp"
#include "dusk/logging.h"
#include "dusk/randomizer/game/randomizer_context.hpp"
#include "m_Do/m_Do_audio.h"

#include <thread>

namespace dusk::ui {

enum class SeedGenerateStatus {
    Ready,
    Generating,
    Success,
    Error,
};

static std::atomic seedGenStatus = SeedGenerateStatus::Ready;
static std::string generationStatusMsg{};

RandomizerGenerationModal::RandomizerGenerationModal() :
    Modal({
       .title = "Randomizer",
       .bodyRml = "Generating Seed...",
       .onDismiss = [this](Modal& modal) {
           mDoAud_seStartMenu(kSoundWindowClose);
           modal.pop();
       },
       .icon = "verifying",
    }) {
    mRoot->SetProperty("white-space", "pre-line");
}

void RandomizerGenerationModal::update() {
    Document::update();

    auto curSeedGenStatus = seedGenStatus.load();

    // Change the modal text if we've finished attempting to generate
    if (curSeedGenStatus == SeedGenerateStatus::Success ||
        curSeedGenStatus == SeedGenerateStatus::Error)
    {
        if (curSeedGenStatus == SeedGenerateStatus::Success) {
            mDoAud_seStartMenu(kSoundSeedGenerateSuccess);
            set_icon("celebration");
        } else {
            mDoAud_seStartMenu(kSoundSeedGenerateError);
            set_icon("error");
        }

        set_body(escape(generationStatusMsg));
        add_action({
            .label = "OK",
            .onPressed = [this](Modal& modal) {
                mDoAud_seStartMenu(kSoundWindowClose);
                modal.pop();
            }
        });

        // Refocus so that we focus the new button
        focus();

        seedGenStatus.store(SeedGenerateStatus::Ready);
    }
}

static void StartSeedGeneration() {
    if (GenerateAndWriteSeed(generationStatusMsg)) {
        seedGenStatus.store(SeedGenerateStatus::Success);
    } else {
        seedGenStatus.store(SeedGenerateStatus::Error);
    }

    DuskLog.debug("{}", generationStatusMsg);
}

void GenerateRandomizerSeed() {
    // Start Generation Thread
    seedGenStatus.store(SeedGenerateStatus::Generating);
    std::thread randoGenerationThread(StartSeedGeneration);
    randoGenerationThread.detach();

    // Create Seed Generation Modal
    push_document(std::make_unique<RandomizerGenerationModal>());

    // Focus Modal
    if (auto* doc = top_document()) {
        doc->focus();
    }
}

}
