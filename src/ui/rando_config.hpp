#pragma once
#include "window.hpp"

namespace dusk::ui {
    class RandomizerWindow  : public Window {
    public:
        RandomizerWindow();

    private:
        bool m_showRandoGeneration = false;
    };
}
