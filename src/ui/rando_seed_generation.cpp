#include "rando_seed_generation.hpp"

#include <mods/svc/log.hpp>

#include "../session.hpp"
#include "../randomizer_context.hpp"

#include "m_Do/m_Do_audio.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace randomizer::ui {
enum class SeedGenerateStatus {
    Ready,
    Generating,
    Success,
    Error,
};

UiDialogHandle seedGenDialog{0};
UiElementHandle seedGenProgressBar{0};
static std::atomic seedGenStatus = SeedGenerateStatus::Ready;
static std::atomic seedGenProgressValue = 0.0f;
static std::string generationStatusMsg{};
static std::mutex generationStatusMutex;

void OnDialogActionOK(ModContext* ctx, UiDialogHandle dialogHandle, void*) {
    mDoAud_seStartMenu(Z2SE_SY_MENU_BACK);
    if (seedGenDialog == dialogHandle) {
        seedGenDialog = 0;
    }
    session::svc_mng.ui->dialog_close(ctx, dialogHandle);
}

static void StartSeedGeneration() {
    if (GenerateAndWriteSeed()) {
        seedGenStatus.store(SeedGenerateStatus::Success);
    } else {
        seedGenStatus.store(SeedGenerateStatus::Error);
    }

    std::string generationStatus = ReadGenerationStatusMsg();
    mods::log::debug("{}", generationStatus);
}

static ModResult buildProgressUpdate(ModContext* ctx, UiElementHandle pane, void*, ModError*) {
    seedGenProgressBar = 0;
    auto result = session::svc_mng.ui->pane_add_progress(ctx, pane, 0.5f, &seedGenProgressBar);
    if (result != MOD_OK) {
        mods::log::error("Failed to create seed generation progress bar");
        return MOD_ERROR;
    }
    return MOD_OK;
}

static ModResult buildDialog() {
    UpdateGenerationStatusMsg("Generating Seed...");

    UiDialogDesc desc = UI_DIALOG_DESC_INIT;
    desc.title = "Generating Randomizer Seed";
    desc.body_rml = "Generating Seed...";
    desc.icon = "verifying";
    desc.variant = UI_DIALOG_NORMAL;
    desc.build = buildProgressUpdate;

    UiDialogAction action = {
        .struct_size = sizeof(UiDialogAction),
        .label = "OK",
        .on_pressed = OnDialogActionOK,
        .user_data = nullptr,
        .keep_open = false,
        .is_disabled = [](ModContext*, void*) {
            // disable button while seed is generating
            return seedGenStatus.load() == SeedGenerateStatus::Generating;
        }
    };
    desc.actions = &action;
    desc.action_count = 1;

    if (session::svc_mng.ui->dialog_push(session::svc_mng.mod_ctx, &desc, &seedGenDialog) != MOD_OK) {
        mods::log::error("Failed to push dialog");
        return MOD_ERROR;
    }

    return MOD_OK;
}

void GenerateRandomizerSeed() {
    if (seedGenStatus.load() != SeedGenerateStatus::Ready) {
        return;
    }
    if (buildDialog() != MOD_OK) {
        return;
    }

    // Start generation thread
    seedGenStatus.store(SeedGenerateStatus::Generating);
    std::thread rando_gen_thread(StartSeedGeneration);
    rando_gen_thread.detach();
}

ModResult UpdateSeedGenerationDialog() {
    if (seedGenDialog == 0) {
        const auto status = seedGenStatus.load();
        if (status == SeedGenerateStatus::Success || status == SeedGenerateStatus::Error) {
            seedGenStatus.store(SeedGenerateStatus::Ready);
        }
        return MOD_OK;
    }

    auto curSeedGenStatus = seedGenStatus.load();
    std::string generationStatus = ReadGenerationStatusMsg();

    auto* ctx = session::svc_mng.mod_ctx;
    auto* ui_svc = session::svc_mng.ui;

    // Update the progress bar if we're still attempting to generate
    if (curSeedGenStatus == SeedGenerateStatus::Generating) {
        ui_svc->elem_set_progress(ctx, seedGenProgressBar, seedGenProgressValue.load(std::memory_order_relaxed));
        ui_svc->dialog_set_body(ctx, seedGenDialog, generationStatus.c_str());
    }
    // Change the modal text if we've finished attempting to generate
    else if (curSeedGenStatus == SeedGenerateStatus::Success ||
             curSeedGenStatus == SeedGenerateStatus::Error)
    {
        if (curSeedGenStatus == SeedGenerateStatus::Success) {
            ui_svc->elem_set_progress(ctx, seedGenProgressBar, 1.0f);
            mDoAud_seStartMenu(Z2SE_SY_FILE_SAVE_OK);
            ui_svc->dialog_set_icon(ctx, seedGenDialog, "celebration");
        } else {
            ui_svc->elem_set_progress(ctx, seedGenProgressBar, 0.0f);
            mDoAud_seStartMenu(Z2SE_SYS_RESULT_WRONG);
            ui_svc->dialog_set_icon(ctx, seedGenDialog, "error");
        }

        ui_svc->dialog_set_body(ctx, seedGenDialog, generationStatus.c_str());
        seedGenStatus.store(SeedGenerateStatus::Ready);
    }

    return MOD_OK;
}

void UpdateSeedGenProgressValue(float progress) {
    seedGenProgressValue.store(progress, std::memory_order_relaxed);
}


void UpdateGenerationStatusMsg(const std::string& str) {
    std::lock_guard guard{generationStatusMutex};
    generationStatusMsg = str;
}

std::string ReadGenerationStatusMsg() {
    std::lock_guard guard{generationStatusMutex};
    return generationStatusMsg;
}

}
