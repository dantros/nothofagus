#pragma once

#include <vector>
#include <string>

namespace Nothofagus
{

/**
 * @class AnimationState
 * @brief Represents the state of an animation, managing the current animation layer and its timing.
 * 
 * The `AnimationState` class is responsible for managing the layers of an animation (which represent
 * different frames or images), the timing between the layers, and keeping track of the current layer
 * being shown. The animation will switch layers based on a defined timing, updating the layer index
 * when the accumulated time exceeds the specified time for the current layer.
 */
class AnimationState
{
    public:

        /**
         * @brief Constructs an `AnimationState` with a list of layers and their transition times.
         * 
         * The constructor initializes the `AnimationState` with a vector of layers and a vector of times.
         * Each layer represents a frame in the animation, and the corresponding time defines how long
         * the layer will be shown before transitioning to the next one.
         * 
         * @param layers A vector of integers representing the layers (frame indices) of the animation.
         * @param times A vector of floats representing the transition time between each layer.
         * @param name The name of the animation state (e.g., "idle", "run").
         */
        AnimationState(const std::vector<int>& layers, const std::vector<float>& times, const std::string& name)
        : mCurrentLayerIndex(0), mTimeAccumulator(0.0f), mName(name)
        {
            // debugCheck(layers.size() == times.size(), "Transition times between frames do not match layers.");
            mLayers = layers;
            mTimes = times;
        }

        /**
         * @brief Updates the current animation frame based on the elapsed time.
         * 
         * This method is called every frame to update the current layer. It checks if the accumulated time
         * has exceeded the time specified for the current layer. If so, it transitions to the next layer.
         * 
         * @param deltaTime The time elapsed since the last update, typically the frame time.
         */
        void update(float deltaTime);

        /**
         * @brief Returns the current layer index.
         * 
         * This method retrieves the current layer index in the animation. The current layer corresponds to
         * the index in the `mLayers` vector, which represents a specific frame or texture to be shown.
         * 
         * @return The index of the current layer.
         */
        int getCurrentLayer() const;

        /**
         * @brief Returns the name of the animation state.
         * 
         * This method returns the name of the current animation state (e.g., "run", "jump").
         * 
         * @return The name of the animation state.
         */
        std::string getName() const;

        /**
         * @brief Resets the animation state to the first layer.
         * 
         * This method resets the animation state to the beginning, resetting the layer index and time accumulator.
         * It is typically used when restarting the animation or when switching to a new state.
         */
        void reset();

    private:

        std::vector<int> mLayers; /**< A vector of integers representing the layers (frames) of the animation. */
        std::vector<float> mTimes; /**< A vector of floats representing the time each layer is displayed. */
        int mCurrentLayerIndex; /**< The current index of the layer being displayed. */
        float mTimeAccumulator; /**< The accumulated time for the current layer. */
        std::string mName; /**< The name of the animation state. */

};

}

