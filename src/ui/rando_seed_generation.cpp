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
static std::mutex seedGenProgressBarMutex;
static std::atomic seedGenStatus = SeedGenerateStatus::Ready;
static std::atomic seedGenProgressValueTarget = 0.0f;
static float seedGenProgressValueCurrent = 0.0f;
static std::string generationStatusMsg{};
static std::mutex generationStatusMutex;

void OnDialogActionOK(ModContext* ctx, UiDialogHandle dialogHandle, void*) {
    mDoAud_seStartMenu(Z2SE_SY_MENU_BACK);
    if (seedGenDialog == dialogHandle) {
        std::lock_guard lock{seedGenProgressBarMutex};
        seedGenDialog = 0;
        seedGenProgressBar = 0;
    }
    session::svc_mng.ui->dialog_close(ctx, dialogHandle);
}

static void StartSeedGeneration(Type type, archi::WorldData apData) {
    if (GenerateAndWriteSeed(type, apData)) {
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

void GenerateRandomizerSeed(Type type /*= "DEFAULT"*/, const archi::WorldData& apData /*= archi::WorldData{}*/) {
    if (seedGenStatus.load() != SeedGenerateStatus::Ready) {
        return;
    }
    if (buildDialog() != MOD_OK) {
        return;
    }

    // Start generation thread
    seedGenStatus.store(SeedGenerateStatus::Generating);
    std::thread rando_gen_thread(StartSeedGeneration, type, apData);
    rando_gen_thread.detach();
    seedGenProgressValueTarget.store(0.f);
}

void UpdateProgressBar() {
    std::lock_guard lock{seedGenProgressBarMutex};
    if (seedGenProgressBar == 0) {
        return;
    }

    static constexpr float kSpeed = 8.0f;

    // Smoothly update the progress bar depending on what the target value is
    static auto prevTime = std::chrono::steady_clock::now();
    auto curTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(curTime - prevTime).count();
    prevTime = curTime;

    float targetValue = seedGenProgressValueTarget.load(std::memory_order_relaxed);
    auto& currentValue = seedGenProgressValueCurrent;
    if (targetValue <= currentValue) {
        currentValue = targetValue;
    } else if (targetValue - currentValue > 0.f) {
        currentValue += (targetValue - currentValue) * (1.0f - std::exp(-kSpeed * deltaTime));
    }

    auto* ctx = session::svc_mng.mod_ctx;
    auto* ui_svc = session::svc_mng.ui;
    ui_svc->elem_set_progress(ctx, seedGenProgressBar, currentValue);
}

void UpdateSeedGenerationDialog() {
    if (seedGenDialog == 0) {
        const auto status = seedGenStatus.load();
        if (status == SeedGenerateStatus::Success || status == SeedGenerateStatus::Error) {
            seedGenStatus.store(SeedGenerateStatus::Ready);
        }
        return;
    }

    auto curSeedGenStatus = seedGenStatus.load();
    std::string generationStatus = ReadGenerationStatusMsg();

    auto* ctx = session::svc_mng.mod_ctx;
    auto* ui_svc = session::svc_mng.ui;

    // Update the progress bar if we're still attempting to generate
    if (curSeedGenStatus == SeedGenerateStatus::Generating) {
        ui_svc->dialog_set_body(ctx, seedGenDialog, generationStatus.c_str());
    }
    // Change the modal text if we've finished attempting to generate
    else if (curSeedGenStatus == SeedGenerateStatus::Success ||
             curSeedGenStatus == SeedGenerateStatus::Error)
    {
        if (curSeedGenStatus == SeedGenerateStatus::Success) {
            seedGenProgressValueTarget.store(1.f);
            mDoAud_seStartMenu(Z2SE_SY_FILE_SAVE_OK);
            ui_svc->dialog_set_icon(ctx, seedGenDialog, "celebration");
        } else {
            seedGenProgressValueTarget.store(0.f);
            mDoAud_seStartMenu(Z2SE_SYS_RESULT_WRONG);
            ui_svc->dialog_set_icon(ctx, seedGenDialog, "error");
        }

        ui_svc->dialog_set_body(ctx, seedGenDialog, generationStatus.c_str());
        seedGenStatus.store(SeedGenerateStatus::Ready);
    }
}

void UpdateSeedGenProgressValue(float progress) {
    seedGenProgressValueTarget.store(progress, std::memory_order_relaxed);
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
