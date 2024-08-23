#pragma once

#include <ciso646>
#include <string>
#include <spdlog/spdlog.h>

namespace Nothofagus
{
    /* Check for the given condition to be valid on debug builds, throws otherwise. */
    inline void debugCheck(bool condition, const std::string& errorMessage)
    {
        #ifdef _DEBUG
        if (not condition)
        {
            spdlog::error("Check failed: {}", errorMessage);
            throw;
        }
        #endif
    }

    /* Check for the given condition to be valid on all runtime builds, throws otherwise. */
    inline void runtimeCheck(bool condition, const std::string& errorMessage)
    {
        if (not condition)
        {
            spdlog::error("Check failed: {}", errorMessage);
            throw;
        }
    }
}