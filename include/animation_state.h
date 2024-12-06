#pragma once

#include <vector>
#include <string>
// #include "check.h"

namespace Nothofagus
{

class AnimationState
{
    public:
        AnimationState(const std::vector<int>& layers, const std::vector<float>& times, const std::string& name)
        : mCurrentLayerIndex(0), mTimeAccumulator(0.0f), mName(name)
        {
            // debugCheck(layers.size() == times.size(), "Transition times between frames do not match layers.");
            mLayers = layers;
            mTimes = times;
        }

        // Update animation frame
        void update(float deltaTime);

        // Get current layer for texture array
        int getCurrentLayer() const;

        // Managing transitions between animation states
        // void transition();

        // Get animation state name
        std::string getName() const;

        // Reseting layer index and time accumulator
        void reset();

    private:

        std::vector<int> mLayers;
        std::vector<float> mTimes;
        int mCurrentLayerIndex;
        float mTimeAccumulator;

        std::string mName;

};

}

