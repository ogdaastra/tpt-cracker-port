#include "simulation/circuits/circuits.h"
#include "simulation/ElementCommon.h"

#include <iomanip>
#include <iostream>

Circuit * circuit_map[NPART];
std::vector<Circuit *> all_circuits;

// Functions
void CIRCUITS::add_circuit(int x, int y, Simulation * sim) {
    int r = sim->photons[y][x];
    if (circuit_map[ID(r)])
        return; // Already part of a circuit

    Circuit * c = new Circuit(x, y, sim);
    if (!c->branch_cache_size()) {
        delete c;
        return;
    }
    all_circuits.push_back(c);
}

void CIRCUITS::delete_circuit(int i) {
    // Why don't we put this in the destructor?
    // Doing so causes extreme lag as invalid circuits are deleted
    // but by reseting circuit_map this causes the invalid circuit to
    // be recreated next frame over and over (ie in blobs of VOLT)
    for (auto id : all_circuits[i]->get_global_rspk_ids())
        circuit_map[id] = nullptr;
    delete all_circuits[i];
    all_circuits.erase(all_circuits.begin() + i);
}

void CIRCUITS::update_all_circuits() {
    for (int i = all_circuits.size() - 1; i >= 0; i--) {
        Circuit * c = all_circuits[i];
        if (c->should_recalc()) {
            c->reset();
            c->generate();
        }
        if (!c->branch_cache_size()) {
            delete_circuit(i);
            continue;
        }
        c->reset_effective_resistances();
        c->solve();
        c->update_sim();
    }
}

void CIRCUITS::clear_circuits() {
    for (auto c : all_circuits)
        delete c;
    all_circuits.clear();
    std::fill(&circuit_map[0], &circuit_map[NPART], nullptr);
}
