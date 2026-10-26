#ifndef CIRCUITS_H
#define CIRCUITS_H

#include "simulation/ElementCommon.h"
#include "simulation/circuits/circuit_core.h"
#include "components/circuit.h"

#include <vector>

class Branch;
class Component;

extern Circuit * circuit_map[NPART_MAX]; // ID to Circuit * (the first NPART are used)
extern std::vector<Circuit *> all_circuits;

namespace CIRCUITS {
    // Functions
    void add_circuit(int x, int y, Simulation * sim);
    void delete_circuit(int i);
    void update_all_circuits();
    void clear_circuits();
}

#endif