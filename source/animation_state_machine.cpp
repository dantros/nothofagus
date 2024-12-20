#include "animation_state_machine.h"
#include "check.h"

namespace Nothofagus {
    // Agregar un nodo de estado de animación
    void AnimationStateMachine::addState(const State& stateName, AnimationState* state) 
    {
        mAnimationStates[stateName] = state;
    }

    // Configurar el estado inicial
    void AnimationStateMachine::setState(const State& stateName) 
    {
        currentState = stateName;
    }

    void AnimationStateMachine::newAnimationTransition(const State& state, const std::string& transition_name, const State& resultingState) 
    {
        debugCheck(mAnimationStates.find(state) != mAnimationStates.end(), "State not in machine.");
        debugCheck(mAnimationStates.find(resultingState) != mAnimationStates.end(), "Resulting state not in machine.");
        transitions[Transition(state, transition_name)] = resultingState;
    }

    // Cambiar de estado (Transición)
    void AnimationStateMachine::transition(const std::string& transition_name) 
    {
        debugCheck(mAnimationStates.find(currentState) != mAnimationStates.end(), "Current state not in machine.");
        currentState = transitions[Transition(currentState, transition_name)];
        mAnimationStates[currentState]->reset();  // Reset state
    }

    void AnimationStateMachine::goToState(const State& stateName)
    {
        debugCheck(mAnimationStates.find(stateName) != mAnimationStates.end(), "New state not in machine.");
        currentState = stateName;
        mAnimationStates[currentState]->reset();  // Reset state
    }

    // Update current state
    void AnimationStateMachine::update(float deltaTime) 
    {
        debugCheck(mAnimationStates.find(currentState) != mAnimationStates.end(), "Current state not in machine.");
        // Update current state
        mAnimationStates[currentState]->update(deltaTime);
        
        // Update current layer
        mAnimatedBellota.setActualLayer(mAnimationStates[currentState]->getCurrentLayer());
        
    }

    // Get current layer
    int AnimationStateMachine::getCurrentLayer() const 
    {
        debugCheck(mAnimationStates.find(currentState) != mAnimationStates.end(), "Current state not in machine.");
        return mAnimationStates.at(currentState)->getCurrentLayer();        
    }
}