#include <unordered_map>
#include <list>
#include <iostream>

#include "simulation/ElementCommon.h"

const float MVSD_GRAVITY = 0.09f;
const float MVSD_NEWTON_GRAVITY = 0.14f;
const float MVSD_AIR = 0.006f;

const float MVSD_MAX_IMPACT_PRESSURE = 10.0f;
const float MVSD_MIN_VELOCITY_TO_DAMAGE = 3.0f;

const float MVSD_BOUNCE = 0.5f;

namespace MOVINGSOLID {
// Collision types
const int STATIC = 0;
const int NONSTATIC = 1;
const int MOVING = 2;

const float MAX_VELOCITY = 15.0f;
const int MAX_SOLID_SIZE = 17176; // Should be no larger than 17176 or it will stack overflow on large solids
const int BIG_SOLID = 10; // Min size for a moving solid to do certain checks, leave it small (<20)

// A collision point
class Collision {
public:
    Collision(int x_, int y_, int type_, int px_, int py_, int cx_, int cy_):
        x(x_), y(y_), px(px_), py(py_), cx(cx_), cy(cy_), type(type_) {
            require_snap = fabs(px - cx) > 2 || fabs(py - cy) > 2;
        };
    int x, y, px, py, cx, cy, type;
    bool require_snap = false;
};

// A group of particles (a moving solid)
class MVSDGroup {
public:
    MVSDGroup();
    MVSDGroup(const std::vector<int> &particles, int type, int id);
    void update(Particle *parts, int pmap[YRES][XRES], Simulation *sim);
    void calc_center(Particle *parts);
    void assign_center(int cx, int cy);
    void add_collision(Collision c) { collisions.push_back(c); }
    void flag_overlap() { another_particle_overlap = true; }
    void should_impact_pressure(bool val) { should_pressure_on_impact = val; }
    void do_big_bounce() { big_bounce = true; }
    void update_delta(int dx_, int dy_) {
        if (abs(dx_) < abs(dx) || !usedx) {
            usedx = true;
            dx = dx_;
        }
        if (abs(dy_) < abs(dy) || !usedy) {
            usedy = true;
            dy = dy_;
        }
    }
    
    void set_velocity(float vx_, float vy_) {
        vx = vx_; vy = vy_;
    }

    // Returns direction of each velocity, basically, -1, 0 or 1
    void get_fake_velocity(int &fvx, int &fvy) {
        fvx = fvy = 0;
        if (vx != 0) fvx = vx < 0 ? -1 : 1;
        if (vy != 0) fvy = vy < 0 ? -1 : 1;
    }

    float getVX() { return vx; }
    float getVY() { return vy; }
    int getCX() { return cx; }
    int getCY() { return cy; }
    int stateID() { return state_id; }
    int particles() { return particle_ids.size(); }
private:
    std::vector<int> particle_ids;
    int ptype, state_id, cx, cy, dx, dy;
    bool usedx = false, usedy = false;
    float vx, vy, fx, fy, pfx, pfy;
    unsigned int previous_collision_size = 0;
    bool should_pressure_on_impact = false; // Make pressure on impact?
    bool another_particle_overlap = false; // Is the MVSD phased into another particle?
    bool big_bounce = false; // Did it hit repulsion gel?

    // Collision handling, saves where the solid has collided with other blocks
    std::vector<Collision> collisions;

    // Helper
    void calc_forces(Particle *parts, int pmap[YRES][XRES], Simulation *sim);
};

// Hash map of all moving solids
extern std::unordered_map<int, MVSDGroup> solids;

// ID handling
int get_new_state();

// Floodfill creation
void create_moving_solid(Particle *parts, int pmap[YRES][XRES], int i, int type=PT_MVSD);
};