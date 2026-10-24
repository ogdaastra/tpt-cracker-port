#ifndef QUANTUM_H
#define QUANTUM_H

// #define DEBUG_QUANTUM_H // Uncomment for debug mode

#include <unordered_map>
#include <vector>

#include "simulation/quantum/linalg.h"
#include "simulation/quantum/def.h"
#include "simulation/ElementCommon.h"

namespace QUANTUM {
    struct QuantumState {
        LINALG::vec state;
        std::vector<int> id_order;
    };

    extern std::unordered_map<int, QuantumState> quantum_states;

    void observe(int state_id, int basis);
    int measure(int particle_id, int state_id, int basis=0);
    void apply_gate(const LINALG::matrix &gate, Particle *parts, std::vector<int> particle_ids);

    void decohere_particle(Particle *parts, int i);
    int create_particle_state(Particle *parts, int i, bool skipdecoherence=false);
};

#endif