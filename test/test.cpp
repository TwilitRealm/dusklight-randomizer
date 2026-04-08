#include "test.hpp"

#include "../logic/generate.hpp"
#include "../logic/world.hpp"
#include "../utility/string.hpp"

#include <filesystem>
#include <iostream>

namespace tphdr::test::test
{
    void RunTests()
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(DATA_PATH "tests/logic"))
        {
            if (entry.path().generic_string().ends_with("settings.yaml"))
            {
                auto pathFolders = tphdr::utility::str::Split(entry.path().generic_string(), '/');
                auto& testName = pathFolders[pathFolders.size() - 2];
                std::filesystem::remove(SETTINGS_PATH);
                std::filesystem::copy_file(entry, SETTINGS_PATH);

                std::cout << "Testing " << testName << std::endl;

                try {
                    tphdr::logic::generate::GenerateWorlds();
                }
                catch(const std::exception& e) {
                    std::cout << "Test \"" << testName << "\" failed! Failed settings saved to " << SETTINGS_PATH << std::endl;
                    std::cout << "Error Message:" << std::endl;
                    throw e;
                }

                std::filesystem::remove(SETTINGS_PATH);
            }
        }
        // Remove test preferences
        std::filesystem::remove(PREFERENCES_PATH);

        std::cout << "All Settings Tests passed" << std::endl;
    }
} // namespace tphdr::test::test
