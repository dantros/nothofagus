#pragma once

#include "animation_state.h"
#include "bellota.h"
#include <iostream>
#include <map>
#include <vector>

/**
 * @namespace Nothofagus
 * @brief The Nothofagus namespace contains all classes and functions used in the animated objects and state machines.
 */
namespace Nothofagus
{

/**
 * @typedef State
 * @brief A string type representing the name of a state in the animation state machine.
 */    
using State = std::string;

/**
 * @typedef Transition
 * @brief A pair representing a transition from one state to another.
 * 
 * The `Transition` pair consists of the current state and the transition event name that triggers the change.
 */
using Transition = std::pair<State, std::string>;

/**
 * @typedef TransitionMap
 * @brief A map of transitions, where the key is a `Transition` pair (current state and transition event), and the value is the resulting state.
 */
using TransitionMap = std::map<Transition, State>;

/**
 * @typedef StatesMap
 * @brief A map of all the states in the animation state machine, where the key is the state's name (a `State`), 
 * and the value is a pointer to the corresponding `AnimationState` object.
 */
using StatesMap = std::map<State, AnimationState*>;


/**
 * @class AnimationStateMachine
 * @brief A class representing a state machine for controlling animations of an `AnimatedBellota`.
 * 
 * This class manages the animation states and transitions of an animated object, `AnimatedBellota`, and updates 
 * the displayed texture layer based on the current state. It provides methods to add states, define transitions, 
 * and update the state machine.
 */
class AnimationStateMachine
{
    public:
        /**
         * @brief Constructs an `AnimationStateMachine` for an animated object.
         * 
         * The constructor takes an `AnimatedBellota` object as a reference and initializes the state machine.
         * 
         * @param animatedBellota A reference to the `AnimatedBellota` object associated with this state machine.
         */
        AnimationStateMachine(Nothofagus::Bellota& animatedBellota) : mAnimatedBellota(animatedBellota) {}

        /**
         * @brief Adds a new animation state to the state machine.
         * 
         * This method allows adding a new state, represented by an `AnimationState` object, to the state machine.
         * 
         * @param stateName The name of the state (a string).
         * @param state A pointer to the `AnimationState` object representing this state.
         */
        void addState(const State& stateName, AnimationState* state);

        /**
         * @brief Sets the initial state of the animation state machine.
         * 
         * This method sets the initial state that the state machine will begin with.
         * 
         * @param stateName The name of the initial state.
         */
        void setState(const State& stateName);

        /**
         * @brief Adds a new transition from one state to another.
         * 
         * This method allows adding a new transition between two states. A transition is triggered by a 
         * specific event (transition name), and results in the state machine changing to the resulting state.
         * 
         * @param state The current state name where the transition starts.
         * @param transition_name The name of the transition event that triggers the change.
         * @param resultingState The state to transition to after the event is triggered.
         */
        void newAnimationTransition(const State& state, const std::string& transition_name, const State& resultingState);
        
        /**
         * @brief Performs a transition from the current state based on the provided transition name.
         * 
         * This method checks the current state and attempts to perform a transition based on the transition name.
         * 
         * @param transition_name The name of the transition event.
         */
        void transition(const std::string& transition_name);

        /**
         * @brief Directly transitions to a specific state.
         * 
         * This method sets the state machine to immediately transition to the specified state, bypassing 
         * any transition events.
         * 
         * @param stateName The name of the state to go to.
         */
        void goToState(const State& stateName);

        /**
         * @brief Updates the current state of the animation state machine.
         * 
         * This method updates the current state, typically by advancing the animation and checking if a transition
         * should occur based on the elapsed time (`deltaTime`).
         * 
         * @param deltaTime The time elapsed since the last update, used to update the state.
         */
        void update(float deltaTime);

        /**
         * @brief Gets the current layer of the active animation state.
         * 
         * This method returns the current texture layer based on the active animation state.
         * 
         * @return The index of the current texture layer.
         */
        int getCurrentLayer() const;

    private:
        StatesMap mAnimationStates; /**< A map of all animation states, keyed by state name. */
        TransitionMap transitions; /**< A map of transitions, keyed by transition event and starting state. */
        State currentState; /**< The name of the current active state. */
        Nothofagus::Bellota& mAnimatedBellota; /**< The reference to the associated `AnimatedBellota` object. */
};

}


