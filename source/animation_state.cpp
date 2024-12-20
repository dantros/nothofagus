#include "animation_state.h"

namespace Nothofagus
{

    void AnimationState::update(float deltaTime) 
    {
        if (mTimeAccumulator >= mTimes[mCurrentLayerIndex]) {
            mTimeAccumulator = 0.0f;
            mCurrentLayerIndex = (mCurrentLayerIndex + 1) % mLayers.size();
        }
        mTimeAccumulator += deltaTime;
    }

    int AnimationState::getCurrentLayer() const {
        return mLayers[mCurrentLayerIndex];
    }

    std::string AnimationState::getName() const 
    { 
        return mName; 
    }

    void AnimationState::reset() {
        mCurrentLayerIndex = 0;
        mTimeAccumulator = 0.0f;
    }

}