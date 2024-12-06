#pragma once

#include "animation_state.h"
#include "bellota.h"
#include <iostream>
#include <map>
#include <vector>
namespace Nothofagus
{
using State = std::string;

using Transition = std::pair<State, std::string>;
using TransitionMap = std::map<Transition, State>;
using StatesMap = std::map<State, AnimationState*>;

class AnimationStateMachine
{
    public:
        // Constructor
        AnimationStateMachine(Nothofagus::AnimatedBellota& animatedBellota) : mAnimatedBellota(animatedBellota) {}

        // Agregar un nodo de estado de animación
        void addState(const State& stateName, AnimationState* state);

        // Configurar el estado inicial
        void setState(const State& stateName);

        // Add state transition
        void newAnimationTransition(const State& state, const std::string& transition_name, const State& resultingState);
        
        // Transition from actual state
        void transition(const std::string& transition_name);

        // Directly go to state
        void goToState(const State& stateName);

        // Actualizar el estado actual
        void update(float deltaTime);

        // Obtener la capa actual del estado
        int getCurrentLayer() const;

    private:
        StatesMap mAnimationStates;
        TransitionMap transitions;
        State currentState;  // Estado actual
        Nothofagus::AnimatedBellota& mAnimatedBellota;
};

}


