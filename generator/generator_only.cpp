#include "randomizer.hpp"

#include <filesystem>

int main() {
    std::filesystem::create_directories(RANDO_SAVE_PATH);
    randomizer::Randomizer rando{randomizer::Type::DEFAULT, RANDO_SAVE_PATH};
    rando.Generate();

    return 0;
}