#ifndef SPACESHIP_H
#define SPACESHIP_H

#define THRUSTER_FORCE 1.0f

#include "simulation/ElementCommon.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <set>

namespace SHIPS {
// Config and stuff
class Spaceship;
const std::vector<int> components({ PT_COTR, PT_NAVI, PT_LASR, PT_HULL, PT_THRS, PT_RADR, PT_FILT });
bool is_component(int comp);
bool is_ship(int id);

// Ship state
extern std::unordered_map<int, Spaceship> ships;
int generate_ship_id();

class Spaceship {
public:
    Spaceship(int id_=-1): id(id_), vx(0.0f), vy(0.0f) {};

    void add_component(int i, int type) {
        components.insert(i);
        component_map[type].insert(i);
    }
    void remove_component(int i, int type) {
        components.erase(i);
        component_map[type].erase(i);
    }
    void destroy(Simulation* sim);

    int id; // Id in the map
    double vx, vy; // Velocity
    std::set<int> components; // Ids of components
    std::unordered_map<int, std::set<int> > component_map; // Map type : set
};

// Ship modification
void move_ship(Simulation* sim, int id);
void calculate_acceleration(Simulation *sim, int id);
int create_ship(int id=-1);
void cloneTMP34(Simulation* sim, int i, int x, int y);
}

#endif