#ifndef CIRCUIT_BRANCH_CLASS
#define CIRCUIT_BRANCH_CLASS

#include "simulation/ElementCommon.h"
#include "simulation/circuits/circuit_core.h"
#include "simulation/circuits/framework.h"

#include <vector>

class Circuit;
class Simulation;
struct BranchConstructionData {
    std::vector<int> ids,
        rspk_ids,
        switches,
        dynamic_resistors;
    double total_resistance = 0.0;
    double voltage_gain = 0.0;
    double current_gain = 0.0f;
    int diode_type = 0;

    ParticleId node1_id, node2_id;
};

// A line between 2 nodes. I have no idea what they're actually called
// so I'll call them branches (or maybe edges)
class Branch {
public:
    /**
     * Resistance is base resistance (only resistors)
     * Effective resistance is dynamic, base is static
     * 
     * Notes on polarity:
     * Voltage: going from node1 to node2 drops by this voltage, ie
     *   NODE1 ----- + | - ------ NODE2
     * is positive voltage
     * 
     * Current: flows from node1 to node2 (positive)
     *   NODE1 -----> NODE2 (positive)
     * 
     * Dioide: going from node1 to node2 is positive_diode
     *   NODE1 ----- PSCN NSCN ------ NODE2
     */

    const NodeId node1, node2;
    const std::vector<ParticleId> ids;
    const std::vector<ParticleId> rspk_ids;
    const std::vector<ParticleId> switches;
    const std::vector<ParticleId> dynamic_resistors;
    const ParticleId node1_id, node2_id;
    const int diode; // 0 = no diode, 1 = positive, -1 = negative

    double resistance, voltage_gain,
        current_gain, base_resistance,
        current=0.0, V1, V2;
    bool recompute_switches = true;
    
    // Divergence checks
    double SS_voltage = std::numeric_limits<double>::max(), SS_current = std::numeric_limits<double>::max();

    Branch(NodeId node1, NodeId node2, BranchConstructionData data) :
        node1(node1), node2(node2), ids(data.ids), rspk_ids(data.rspk_ids), switches(data.switches),
        dynamic_resistors(data.dynamic_resistors), node1_id(data.node1_id), node2_id(data.node2_id),
        diode(data.diode_type), resistance(data.total_resistance), voltage_gain(data.voltage_gain),
        current_gain(data.current_gain), base_resistance(data.total_resistance) {}
    
    void set_special_type(bool is_capacitor, bool is_inductor, bool is_chip);
    void set_to_steady_state();

    void compute_dynamic_resistances(Simulation * sim, Circuit * c);
    void compute_dynamic_voltages(Simulation * sim, Circuit * c);
    void compute_dynamic_currents(Simulation * sim, Circuit * c);

    bool obeys_ohms_law()    { return obeys_ohms_law_; };
    bool is_voltage_source() { return voltage_gain; }
    bool requires_numeric_integration(Simulation * sim) {
        return ids.size() && is_integration_particle(sim->parts[ids[0]].type); }

    bool is_inductor()  { return branch_type == INDUCTOR; }
    bool is_capacitor() { return branch_type == CAPACITOR; }
    bool is_chip()      { return branch_type == CHIP; }
    bool is_diode()     { return branch_type == DIODE; }

    void print();
private:
    bool obeys_ohms_law_ = true;
    bool switches_on_ = false;
    bool switches_on(Simulation * sim);

    enum TYPE { RESISTOR, CAPACITOR, INDUCTOR, CHIP, DIODE, VOLTAGE_SOURCE };
    TYPE branch_type = RESISTOR;    
};

#endif