#include <unordered_map>
#include <set>

#include "simulation/quantum/quantum.h"
#include "simulation/quantum/linalg.h"
#include "simulation/quantum/def.h"
#include "simulation/quantum/qmath.h"
#include "simulation/ElementCommon.h"

#include <iostream>

using LINALG::matrix;
using LINALG::vec;

namespace QUANTUM {
const int MAX_STATES = 1000000000; // Max quantum states, this is about 10000x max particle count (without layering)
const int MAX_STATE_SIZE = 64; // Max 6 qbit states (2^6 = 64)

/* The quantum state of every particle is saved in this variable
 * It maps the particle state ID (multi particle systems share a state ID)
 * to a given state vector. */
std::unordered_map<int, QuantumState> quantum_states;

/**
 * Returns a new state ID that isn't already used
 */
int get_new_state_id() {
    int state;
    do {
        state = RNG::Ref().between(0, MAX_STATES);
        // State doesn't already exist
        if (quantum_states.count(state) == 0)
            break;
    } while (true);
    return state;
}

/**
 * Assigns a particle a quantum state, use when a new
 * ION is created. Returns new state id
 */
int create_particle_state(Particle *parts, int i, bool skipdecoherence) {
    if (parts[i].type != PT_ION) return -1;
    if (!skipdecoherence) // Avoid infinite loop in decohere function
        decohere_particle(parts, i);

    QuantumState nstate;
    int stateid = get_new_state_id();

    // By default qbits are |0>
    nstate.state.push_back(1.0);
    nstate.state.push_back(0.0);
    nstate.id_order.push_back(i);
    quantum_states[stateid] = nstate;
    parts[i].tmp2 = stateid;
    return stateid;
}

/**
 * Observe a given quantum state (given by state_id, quantum_states[state_id])
 * Upon observing a quantum state the entire state will collapse, meaning for
 * multi-particle states all particles will be affected, even if they're not
 * measured. Does not actually return any state values
 * 
 * Basis: 0=z, 1=y, 2=x
 */
void observe(int state_id, int basis) {
    if (quantum_states.find(state_id) == quantum_states.end())
        throw "Quantum state does not exist\n";

    // Pick a random quantum state to collapse to
    double randomNum = RNG::Ref().uniform01();
    double cprob = 0; // Cumulative probability
    unsigned int i = 0;
    for (; i < quantum_states[state_id].state.size(); ++i) {
        cprob += abs(quantum_states[state_id].state[i]) * abs(quantum_states[state_id].state[i]);
        if (randomNum < cprob) 
            break;
    }

    // Assign new state values
    for (unsigned int n = 0; n < quantum_states[state_id].state.size(); ++n)
        quantum_states[state_id].state[n] = std::complex<double>(0, 0);
    quantum_states[state_id].state[i] = std::complex<double>(1, 0);
}

/**
 * Actually measure a specific particle's state. Will
 * collapse the state of any particles that are entangled
 * or in the same state as it. Returns particle's state
 */
int measure(int particle_id, int state_id, int basis) {
    if (quantum_states.find(state_id) == quantum_states.end())
        return 0; // Invalid state

    observe(state_id, basis);

    // Find the collapsed state and return the particle value
    for (unsigned int i = 0; i < quantum_states[state_id].state.size(); ++i) {
        if (quantum_states[state_id].state[i] == 1.0) { // The collapsed state
            /* The particle state is encoded in the nth binary digit of the index
             * of the state. For example, if state vector was [0, 1, 0, 0]
             *                                                00  01 10 11
             * (2 qbit state) the index below would be 01. If we wished to measure
             * the 2nd digit, we would take the 2nd binary digit, which is 1, and return it
             */
            int index = -std::distance(std::find(
                quantum_states[state_id].id_order.begin(),
                quantum_states[state_id].id_order.end(),
                particle_id), quantum_states[state_id].id_order.begin());

            #ifdef DEBUG_QUANTUM_H
                std::cout << "\n";
                std::cout << "Measuring particle " << particle_id << " with state id " << state_id << "\n";
                std::cout << "This particle exists at index " << index << "\n";
                std::cout << "Measured spin: " << (((1 << index) & i) ? 1 : 0) << "\n";
                
            #endif

            return ((1 << index) & i) ? 1 : 0;
        }
    }
    return 0; // This should never happen but oh well
}



/**
 * Applies the given matrix to the quantum state (given by state_id, quantum_states[state_id])
 * If the matrix size is not large enough it will automatically adjust the matrix
 * size by taking the kronecker product with I
 * 
 * (If vector is not large enough will throw an error however)
 */
void apply_gate_raw(const matrix &gate, int state_id, const std::vector<int> &particle_ids) {
    if (quantum_states.find(state_id) == quantum_states.end())
        throw "Quantum state does not exist\n";
    if (quantum_states[state_id].state.size() < gate.size())
        throw "Quantum state vector is too small for this gate matrix\n";

    // Adjust gate matrix size as needed
    matrix newgate;
    int qstate_size = quantum_states[state_id].state.size();

    if ((int)gate.size() < qstate_size) {
        /* We'll assume all arguments of the gate are the first n particles for an 
         * n particle gate. This is of course, not the case, so we'll swap any out of 
         * place particles. For example, if we wish to apply a CNOT gate (2 qbit input)
         * to the 1st and 3rd qbit in a 3 qbit state, we note that:
         * - 1st qbit: (already in place, don't do anything)
         * - 3rd qbit: (swap with 2nd qbit)
         * 
         * Once all the qbits are swapped so the qbits we wish to apply the gate to
         * are the 0th, 1st, 2nd, ... nth qbits, we can simply do the
         * GATE (X) I (X) I... until we get a matrix of the right size
         * 
         * After that, we swap the quantum states back.
         */
        std::set<std::pair<int, int> > swaps;

        // Do the initial swap
        for (unsigned int i = 0; i < particle_ids.size(); ++i) {
            // Find the position the particle is in the state array
            auto itr = std::find(quantum_states[state_id].id_order.begin(),
                                 quantum_states[state_id].id_order.end(),
                                 particle_ids[i]);
            if (itr == quantum_states[state_id].id_order.end())
                throw "Could not find quantum state id \n";
            unsigned int j = std::distance(quantum_states[state_id].id_order.begin(), itr);

            // If the particle already belongs don't bother swapping
            if (i == j) continue;

            // Swap the particles that differ at position i and j
            QMATH::swap_state_vector(quantum_states[state_id].state, swaps, i, j);
        }

        // Create the new quantum gate
        matrix identity = LINALG::get_identity(2);
        newgate = gate;
        while ((int)newgate.size() < qstate_size) {
            newgate = LINALG::kronecker_product(newgate, identity);
        }

        // Sanity check
        if ((int)newgate.size() > qstate_size)
            throw "New quantum gate size is larger than size of state (This should never happen!)\n";
        else if ((int)newgate.size() < qstate_size)
            throw "New quantum gate size is smaller than size of state (This should never happen!)\n";

        // Apply the new gate
        quantum_states[state_id].state = LINALG::mult_matrix_vec(newgate, quantum_states[state_id].state);

        // Swap everything back
        std::set<std::pair<int, int> > temp; // Garbage set to hold duplicates when swapping
        for (auto itr = swaps.begin(); itr != swaps.end(); ++itr) {
            QMATH::swap_state_vector(quantum_states[state_id].state, temp, itr->first, itr->second);
        }
    } else {
        // The quantum gate is correct size, no need to adjust
        quantum_states[state_id].state = LINALG::mult_matrix_vec(gate, quantum_states[state_id].state);
    }
}

/**
 * Applies a given matrix to the array of particle IDS by first creating a new quantum state
 * formed from each particle ID's states and assigning each particle the new state, then using
 * the apply_gate function below to apply the gate to the new state
 */
void apply_gate(const LINALG::matrix &gate, Particle *parts, std::vector<int> particle_ids) {
    if (particle_ids.size() == 0)
        return;
    if (quantum_states.find(parts[particle_ids[0]].tmp2) == quantum_states.end())
        return;

    QuantumState new_state = quantum_states[parts[particle_ids[0]].tmp2];

    // Decohere invalid states (state size = 0)
    if (new_state.state.size() == 0) {
        decohere_particle(parts, particle_ids[0]);
        return;
    }

    int temp_id;
    for (unsigned int i = 1; i < particle_ids.size(); ++i) {
        temp_id = parts[particle_ids[i]].tmp2;
        if (temp_id == parts[particle_ids[0]].tmp2) // Particles are already grouped together
            continue;
            
        // Merge states, new_state = new (X) current, and
        // ID orders are simply concanated
        new_state.state = LINALG::tensor_product(new_state.state, quantum_states[temp_id].state);
        new_state.id_order.insert(new_state.id_order.end(),
            quantum_states[temp_id].id_order.begin(), quantum_states[temp_id].id_order.end());
    }

    int new_state_id = get_new_state_id();
    quantum_states[new_state_id] = new_state;

    // Set all particles to the new state and delete their old states
    for (unsigned int i = 0; i < quantum_states[new_state_id].id_order.size(); ++i) {
        int id = quantum_states[new_state_id].id_order[i];
        if (parts[id].tmp2 == new_state_id)
            continue; // Don't erase new state by accident

        // Sanity check, uncomment if game is crashing
        // if (parts[id].type != PT_ION) {
        //     #ifdef DEBUG_QUANTUM_H
        //     std::cout << "Particle is not of type ION, decohering\n";
        //     #endif

        //     decohere_particle(parts, particle_ids[0]);
        //     return;
        // }

        auto itr = quantum_states.find(parts[id].tmp2);
        if (itr != quantum_states.end())
            itr = quantum_states.erase(itr);
        parts[id].tmp2 = new_state_id;
    }

    try { 
        apply_gate_raw(gate, new_state_id, particle_ids);
    } catch(char const* e) {
        // We'll ignore errors for now
        // If in debug mode print
        #ifdef DEBUG_QUANTUM_H
            std::cout << "Error " << e << "\n"; 
        #endif
    } 
    #ifdef DEBUG_QUANTUM_H
    catch(...) {
        std::cout << "An unknown error has occured while applying gate of size " << gate.size() << "\n";
    }
    #endif

    // Collapse state if too big
    if (quantum_states[new_state_id].state.size() > MAX_STATE_SIZE) {
        for (unsigned int i = 0; i < quantum_states[new_state_id].id_order.size(); ++i)
            parts[quantum_states[new_state_id].id_order[i]].flags = 0; // Force regeneration of state
        quantum_states.erase(quantum_states.find(new_state_id));

        #ifdef DEBUG_QUANTUM_H
        std::cout << "Collapsed state " << new_state_id << ", size too big\n";
        #endif
    }

    // Debug: print the state
    #ifdef DEBUG_QUANTUM_H
        std::cout  << "Currently applied " << gate.size() << " sized gate  "
                  << quantum_states[new_state_id].state.size() << " <- new size   "
                  << new_state_id << " <- new state\n";
        QuantumState temp = quantum_states[new_state_id];
        for (unsigned int i = 0; i < temp.state.size();i++)
            std::cout << temp.state[i] << " ";
        std::cout << "\n\n";
    #endif
}

/**
 * Triggers decoherence in a particle. Basically, finds the state
 * that contains the particle, and seperates each particle
 * into a random state
 */
void decohere_particle(Particle *parts, int i) {
    auto itr = quantum_states.find(parts[i].tmp2);
    if (itr == quantum_states.end())
        return;

    #ifdef DEBUG_QUANTUM_H
    std::cout << "Particle with ID " << i << " decohered\n";
    #endif

    QuantumState state = quantum_states[parts[i].tmp2];
    for (unsigned int i = 0; i < state.id_order.size(); ++i) {
        if (parts[state.id_order[i]].type != PT_ION)
            continue;
        int t = create_particle_state(parts, state.id_order[i], true);

        // Randomize the quantum state. We pick 1 number rand1 + rand2 * i
        // then pick a 2nd number where |n2|^2 + |n1|^2 = 1
        double rand1 = 0.707 * RNG::Ref().uniform01(), rand2 = 0.707 * RNG::Ref().uniform01();
        double number_2_adds_to = 1 - rand1 * rand1 - rand2 * rand2;

        // Right now rand3 and rand4 are square of their actual values
        double rand3 = number_2_adds_to * RNG::Ref().uniform01();
        double rand4 = number_2_adds_to - rand3;

        std::complex<double> n1(rand1, rand2);
        std::complex<double> n2(sqrt(rand3), sqrt(rand4));

        quantum_states[t].state[0] = n1;
        quantum_states[t].state[1] = n2;
    }
    quantum_states.erase(itr);
}


};
