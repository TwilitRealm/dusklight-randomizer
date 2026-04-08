#pragma once

namespace tphdr::utility::general
{
    template<typename First, typename... T>
    bool IsAnyOf(First&& first, T&&... t)
    {
        return ((first == t) || ...);
    }
} // namespace tphdr::utility::general