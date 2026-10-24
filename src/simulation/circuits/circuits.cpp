#include "simulation/circuits/circuits.h"
#include "simulation/ElementCommon.h"

#include <iomanip>
#include <iostream>

Circuit * circuit_map[NPART];
std::vector<Circuit *> all_circuits;
// sim->timer + 1 of the last attempt that found no valid circuit through this pixel. Without it every
// VOLT/CPTR/LOGC pixel of a blob that is not a circuit floods the whole blob again every frame.
static unsigned int failed_at[YRES][XRES];

// Functions
void CIRCUITS::add_circuit(int x, int y, Simulation * sim) {
    int r = sim->photons[y][x];
    if (circuit_map[ID(r)])
        return; // Already part of a circuit
    if (failed_at[y][x] && sim->timer + 1 - failed_at[y][x] < (unsigned int)FORCE_RECALC_EVERY_N_FRAMES)
        return; // Tried recently, retry later

    Circuit * c = new Circuit(x, y, sim);
    if (!c->branch_cache_size()) {
        for (auto id : c->get_global_rspk_ids()) {
            int px = (int)(sim->parts[id].x + 0.5f), py = (int)(sim->parts[id].y + 0.5f);
            if (px >= 0 && py >= 0 && px < XRES && py < YRES)
                failed_at[py][px] = sim->timer + 1;
        }
        failed_at[y][x] = sim->timer + 1;
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
    std::fill(&failed_at[0][0], &failed_at[0][0] + YRES * XRES, 0u);
}
