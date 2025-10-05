#pragma once

#include <ciso646>
#include <string>
#include <spdlog/spdlog.h>

namespace Nothofagus
{
    /**
     * @brief Checks if a given condition is valid during debug builds, throwing an exception otherwise.
     * 
     * This function is only active in debug builds. If the given condition is not met, it logs an error 
     * and throws an exception. It helps ensure that the code behaves as expected during the development phase.
     * 
     * @param condition The condition to check.
     * @param errorMessage The error message to log if the condition is not valid.
     */
    /* Check for the given condition to be valid on debug builds, throws otherwise. */
    inline void debugCheck(bool condition, const std::string& errorMessage)
    {
        #ifdef _DEBUG
        runtimeCheck(condition, errorMessage);
        #endif
    }

    inline void debugCheck(bool condition)
    {
        debugCheck(condition, "");
    }

    /**
     * @brief Checks if a given condition is valid during runtime, throwing an exception otherwise.
     * 
     * This function is always active, regardless of the build type. It checks the given condition and if it's 
     * not met, it logs an error and throws an exception. It is useful for runtime validation in all builds.
     * 
     * @param condition The condition to check.
     * @param errorMessage The error message to log if the condition is not valid.
     */
    /* Check for the given condition to be valid on all runtime builds, throws otherwise. */
    inline void runtimeCheck(bool condition, const std::string& errorMessage)
    {
        if (not condition)
        {
            spdlog::error("Check failed: {}", errorMessage);
            throw;
        }
    }

    inline void runtimeCheck(bool condition)
    {
        runtimeCheck(condition, "");
    }
}