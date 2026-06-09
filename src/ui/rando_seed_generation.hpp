#pragma once
#include "modal.hpp"

namespace dusk::ui {

class RandomizerGenerationModal : public Modal {
public:
    explicit RandomizerGenerationModal();

    void update() override;
};

void GenerateRandomizerSeed();

}
