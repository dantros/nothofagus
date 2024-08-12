#include <ciso646>
#include <string>
#include <spdlog/spdlog.h>

namespace Nothofagus
{
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

    inline void runtimeCheck(bool condition, const std::string& errorMessage)
    {
        if (not condition)
        {
            spdlog::error("Check failed: {}", errorMessage);
            throw;
        }
    }
}