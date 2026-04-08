#pragma once

#include "config.hpp"

namespace tphdr::seedgen::seed
{
    /**
     *  @brief Generates a random sequence of 3 words to be used as a seed.
     *
     *  @return The sequence of words as a string
     */
    std::string GenerateSeed();

    /**
     *  @brief Generates a random sequence of 3 nouns to be used as a verification hash.
     *
     *  @return The sequence of words as a string
     */
    std::string GenerateHash();

    std::string HashForConfig(const tphdr::seedgen::config::Config& config);
} // namespace tphdr::seedgen::seed
