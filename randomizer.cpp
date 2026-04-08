#include "randomizer.hpp"

#include "logic/generate.hpp"
#include "logic/world.hpp"
#include "test/test.hpp"
#include "utility/log.hpp"

#include <iostream>

int randomizerMain()
{
    try
    {
#ifdef LOGIC_TESTS
        tphdr::test::test::RunTests();
        return 0;
#else
        auto worlds = tphdr::logic::generate::GenerateWorlds();
#endif
    }
    catch(const std::exception& e)
    {
        std::cout << "============================================================" << std::endl;
        std::cout << "The following exception occured: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
