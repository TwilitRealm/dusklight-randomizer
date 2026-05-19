

#ifndef DUSK_IMGUI_MENU_RANDOMIZER_HPP
#define DUSK_IMGUI_MENU_RANDOMIZER_HPP

namespace dusk {
class ImGuiMenuRandomizer {
public:
    ImGuiMenuRandomizer();
    void draw();

    void windowRandoStats();
    void windowRandoGeneration();

private:
    bool m_showRandoStats{false};
    bool m_showRandoGeneration{false};
};
}

#endif //DUSK_IMGUI_MENU_RANDOMIZER_HPP
