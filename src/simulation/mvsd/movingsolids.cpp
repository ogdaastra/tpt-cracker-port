#include <unordered_map>
#include <list>
#include <iostream>
#include <cmath>

#include "simulation/mvsd/mvsd_math.h"
#include "simulation/ElementCommon.h"
#include "simulation/mvsd/movingsolids.h"

#include "simulation/FixedUtils.h"


// A group of moving solids
MOVINGSOLID::MVSDGroup::MVSDGroup() {

}

MOVINGSOLID::MVSDGroup::MVSDGroup(const std::vector<int> &particles, int type, int id) {
    particle_ids = particles;
    vx = vy = fx = fy = 0.0f;
    state_id = id;
    ptype = type;
    dx = dy = 0;
}

// Recalculate the center of mass
void MOVINGSOLID::MVSDGroup::calc_center(Particle *parts) {
    if (particle_ids.size() == 0)
        return;

    int sumx = 0, sumy = 0, count = 0, index = 0;
    for (auto itr = particle_ids.begin(); itr != particle_ids.end(); ++itr) {
        // Speed up calculation: for large solids only consider every 3rd particle
        ++index;
        if (particle_ids.size() > BIG_SOLID && index % 2 == 0)
            continue;

        sumx += (int)(parts[*itr].x + 0.5);
        sumy += (int)(parts[*itr].y + 0.5);
        count++;
    }

    cx = sumx / count;
    cy = sumy / count;
}

// Redefine center of mass
void MOVINGSOLID::MVSDGroup::assign_center(int _cx, int _cy) {
    cx =  _cx; cy = _cy;
}

// Compute forces due to gravity, pressure, etc...
void MOVINGSOLID::MVSDGroup::calc_forces(Particle *parts, GridView<int> pmap, Simulation *sim) {
    // calculate net torque on solid and net force and
    // move particles with the force and rotate the entire solid


    // The center can lie outside the screen for a solid touching the edge; sample the fields inside it
    int fcx = cx < 0 ? 0 : (cx >= XRES ? XRES - 1 : cx), fcy = cy < 0 ? 0 : (cy >= YRES ? YRES - 1 : cy);

    // We can pretend gravity acts on the center of mass
    switch (sim->gravityMode) {
        case 0: { // Downwards gravity
            fy = MVSD_GRAVITY;
            break;
        }
        case 1: {  break; } // No gravity
        case 2: { // Radial gravity
            float pGravD = 0.01f - hypotf((cx - XCNTR), (cy - YCNTR));
            fx = MVSD_GRAVITY * ((float)(cx - XCNTR) / pGravD);
            fy = MVSD_GRAVITY * ((float)(cy - YCNTR) / pGravD);
            break;
        }
    }

    // Newtonian gravity
    fx += MVSD_NEWTON_GRAVITY * sim->gravx[(fcy / CELL) * (XRES / CELL) + (fcx / CELL)];
    fy += MVSD_NEWTON_GRAVITY * sim->gravy[(fcy / CELL) * (XRES / CELL) + (fcx / CELL)];

    // Pressure
    fx += MVSD_AIR * sim->vx[fcy / CELL][fcx / CELL];
    fy += MVSD_AIR * sim->vy[fcy / CELL][fcx / CELL];

    // Stasis field (Yes the checks are needed)
    if (sim->stasis->strength[fcy/STASIS_CELL][fcx/STASIS_CELL] != 0) {
        fx += (sim->stasis->vx[fcy / STASIS_CELL][fcx / STASIS_CELL] - vx) * sim->stasis->strength[fcy/STASIS_CELL][fcx/STASIS_CELL];
        fy += (sim->stasis->vy[fcy / STASIS_CELL][fcx / STASIS_CELL] - vy) * sim->stasis->strength[fcy/STASIS_CELL][fcx/STASIS_CELL];
    }

    // Apply the net force (it's really an acceleration tbh)
    vx += fx;
    vy += fy;
}

void MOVINGSOLID::MVSDGroup::update(Particle *parts, GridView<int> pmap, Simulation *sim) {
    if (particle_ids.size() == 0)
        return;

    // Collision handling
    // ---------------------------------------------
    bool should_bounce = false; // Should the particle bounce off the surface or ignore bounce
    bool should_freeze = false; // Should the particle freeze? (ie on a surface) or ignore
    bool velocity_is_fast = (fabs(vx) > 0.5 || fabs(vy) > 0.5); // Fast enough to bounce?

    float deflectx = 0, deflecty = 0; // Keep track of deflection dir, this is like a "net force" vector
    int total_repel_x = 0, total_repel_y = 0; // Tracks direction to "repel" away from other particles / solids
    float rotate_sum = 0; // Keep track of total torque

    int fake_vx, fake_vy; // -1, 0, or 1 representing direction of travel
    float v_mag = sqrtf(vx * vx + vy * vy);
    get_fake_velocity(fake_vx, fake_vy);

    // Fake ass momentum conservation
    std::vector<MVSDGroup*> moving_solids_to_bounce;

    for (auto i = collisions.begin(); i != collisions.end(); ++i) {
        /**
         * Here we distinct between merely being "beside"
         * and touching an object and actually colliding with it (ie, is there
         * a normal force from the collision?
         * 
         * If a particle exists 1px in the direction of the velocity vector, OR
         * the displacement particle that detected the collision to the collided particle
         * is nearly along the current velocity vector, we have collided
         */
        
        // Determine if moving in the direction of vx or vy will intersect another particle
        // Since high speed collisions are handled by MVSD we can assume vx and vy are not that big
        // and just check the location directly without doing a line intersection
        int testx = (int)round(i->cx + fake_vx);
        int testy = (int)round(i->cy + fake_vy);
        if (testx < 0 || testy < 0 || testx >= XRES || testy >= YRES)
            continue;

        // Should collision be super-bouncy? (If portal gel ctype)
        if (parts[ID(pmap[i->py][i->px])].ctype == PT_PGEL)
            big_bounce = true;

        int r = pmap[testy][testx];
        MVSDGroup *other_solid = &solids[parts[ID(pmap[i->cy][i->cx])].tmp2];

        if ((i->cx - i->px == fake_vx && i->cy - i->py == fake_vy) || 
            (r && (TYP(r) != ptype || parts[ID(r)].tmp2 != state_id))) {
            bool not_other_is_small_mvsd = (i->type != MOVING || other_solid->particles() > BIG_SOLID);

            // If we're approaching the other object at "high speed" then bounce
            if (velocity_is_fast && not_other_is_small_mvsd) {
                should_bounce = true;
            }

            // Since it's a collision and if the solid continues on its current velocity
            // it will intersect another particle we mark that it should stop (since it's
            // not bouncing)
            else if (!should_bounce && not_other_is_small_mvsd) {
                should_freeze = true;
                total_repel_x += i->px - i->cx;
                total_repel_y += i->py - i->cy;
            }

            // Update net force direction
            deflectx -= i->cx - i->px;
            deflecty -= i->cy - i->py;

            // Update net torque
        }

        // Collision with another moving solid
        // Determine if we should deflect it
        if (i->type == MOVING && velocity_is_fast) {
            if (!(other_solid->vx || other_solid->vy)) {
                moving_solids_to_bounce.push_back(other_solid);
            }
        }

        // Collision with powder
        else if (i->type == NONSTATIC) {
            r = ID(pmap[i->cy][i->cx]);
            if (parts[r].type == ptype) continue;

            parts[r].vx += vx * 2;
            parts[r].vy += vy * 2;
        }
        
        // Make impact on fall
        if (should_pressure_on_impact && collisions.size() != previous_collision_size && v_mag > MVSD_MIN_VELOCITY_TO_DAMAGE)
            sim->pv[i->cy / CELL][i->cx / CELL] += MVSD_MAX_IMPACT_PRESSURE * v_mag / (1.141 * MAX_VELOCITY);
    }

    // To prevent phasing through solids we negate solids that should
    // be stopped (ie resting on a surface). Every 50 frames we
    // randomly impart a repeling velocity from the direction of objects
    // it collided with (DO NOT CHANGE THE MAGIC CONSTANTS BELOW)
    // to avoid particles being "stuck" inside one another
    // (We also apply this repeling force during a force change)
    if (should_freeze) {
        if (sim->timer % 50 == 0 || fx != pfx || fy != pfy || another_particle_overlap) { 
            vx = total_repel_x / 15.0f / collisions.size();
            vy = total_repel_y / 15.0f / collisions.size();
        } else {
            vx = vy = 0.0f;
        }
    }

    // Solid should bounce. If a dx or dy is indicated the solid will bounce
    // anyways because that only happens if velocity is extremely large and solid
    // risks phasing into solids
    float angle = fast_atan2(deflecty, deflectx);
    
    if (!should_freeze && (usedx || usedy || should_bounce)) {
        // Only bounce if deflect is large enough, otherwise you might get
        // extreme deflection angles from 1 or 2px of collision
        if (deflectx || deflecty) {
            if (big_bounce) v_mag *= 2.0f;
            vx = v_mag * cos(angle) * MVSD_BOUNCE;
            vy = v_mag * sin(angle) * MVSD_BOUNCE;
        }
    }

    for (auto i = moving_solids_to_bounce.begin(); i != moving_solids_to_bounce.end(); ++i) {
        (*i)->vx = v_mag * cos(angle + 3.1415) * MVSD_BOUNCE;
        (*i)->vy = v_mag * sin(angle + 3.1415) * MVSD_BOUNCE;
    }

    // Limit max velocity
    if (fabs(vx) > MAX_VELOCITY)
        vx = vx < 0 ? -MAX_VELOCITY : MAX_VELOCITY;
    if (fabs(vy) > MAX_VELOCITY)
        vy = vy < 0 ? -MAX_VELOCITY : MAX_VELOCITY;


    // Update all individual particles
    // ---------------------------------------------
    auto i = particle_ids.begin();
    bool particle_removed = false;

    while (i != particle_ids.end()) {
        // Particle no longer exists, delete it
        if (parts[*i].type != ptype || parts[*i].tmp2 != state_id) {
            i = particle_ids.erase(i);
            particle_removed = true;
            continue;
        }

        // If dx or dy is enabled that means the particle will
        // "snap" to a new location (ie when velocity is fast and
        // causes phasing)
        if (usedx)
            parts[*i].x += dx;
        else
            parts[*i].x += vx;
        if (usedy)
            parts[*i].y += dy;
        else
            parts[*i].y += vy;

        // Warns about rubberbanding (debug)
        // if (abs(px - parts[*i].x) + abs(py - parts[*i].y) > 50) {
        //     std::cout << px << ", " << py << " -> " << parts[*i].x << ", " << parts[*i].y << "\n";
        //     std::cout << vx << ", " << vy << ", delta: " << dx << ", " << dy << "\n";
        // }
        
        if (i != particle_ids.end()) ++i;
    }

    // Trigger recalculation of groups if particles were removed
    // In case, for example, solid was cut
    if (particle_removed && particle_ids.size() > 0) {
        parts[particle_ids[0]].flags = 1;
        parts[particle_ids[0]].tmp3 = TO_FIXED_E(vx, PT_MVSD);
        parts[particle_ids[0]].tmp4 = TO_FIXED_E(vy, PT_MVSD);
    }

    // We can approximate the center of gravity update
    // However the below applied value is not 100% accurate
    // due to rounding error so we have to recalculate
    // every couple of frames to avoid drift. 
    if (sim->timer % 10 == 0) {
        calc_center(parts);
    } else {
        cx = (int)(cx + vx + 0.5);
        cy = (int)(cy + vy + 0.5);
    }
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx > XRES) cx = XRES;
    if (cy > YRES) cy = YRES;

    // Reset everything for next cycle
    // ---------------------------------------------
    dx = dy = 100;
    usedx = false, usedy = false;
    another_particle_overlap = big_bounce = false;
    previous_collision_size = collisions.size();
    collisions.clear();

    pfx = fx; pfy = fy;
    fx = fy = 0.0;
    calc_forces(parts, pmap, sim);
}

// Everything else
namespace MOVINGSOLID {

// Hash map of all moving solids
std::unordered_map<int, MVSDGroup> solids;

// ID handling
int get_new_state() {
    int new_state_id;
    do {
        new_state_id = RNG::Ref().between(1, 10000000);
        if (solids.find(new_state_id) == solids.end())
            break;
    } while(true);
    return new_state_id;
}

// Floodfill creation
// Helper to floodfill
void floodfill_helper(Particle *parts, int x, int y, GridView<int> pmap, int state_id,
        std::vector<int> &to_add_id, int &sumx, int &sumy, int &count, int type=PT_MVSD) {
    if (x < 0 || y < 0 || x > XRES || y > YRES)
        return;

    // Check if current spot is valid, if not return now
    // Invalid if not right type or already has ID (tmp2 > 0)
    if (parts[ID(pmap[y][x])].type != type || parts[ID(pmap[y][x])].tmp2 > 0)
        return;

    parts[ID(pmap[y][x])].tmp2 = state_id;
    to_add_id.push_back(ID(pmap[y][x]));
    
    if (count >= MAX_SOLID_SIZE) // Solids larger than this cause stack overflow lol
        return;

    ++count;
    sumx += (int)(parts[ID(pmap[y][x])].x + 0.5);
    sumy += (int)(parts[ID(pmap[y][x])].y + 0.5);

    floodfill_helper(parts, x - 1, y, pmap, state_id, to_add_id, sumx, sumy, count, type);
    floodfill_helper(parts, x + 1, y, pmap, state_id, to_add_id, sumx, sumy, count, type);
    floodfill_helper(parts, x, y - 1, pmap, state_id, to_add_id, sumx, sumy, count, type);
    floodfill_helper(parts, x, y + 1, pmap, state_id, to_add_id, sumx, sumy, count, type);
}


void create_moving_solid(Particle *parts, GridView<int> pmap, int i, int type) {
    const int state_id = get_new_state();
    std::vector<int> particle_ids;

    int sumx, sumy, n;
    sumx = sumy = n = 0;

    floodfill_helper(parts, (int)(parts[i].x + 0.5), (int)(parts[i].y + 0.5), pmap, state_id, particle_ids,
        sumx, sumy, n, type);
    if (n <= 0) return; // We found nothing, can occur when stamps trigger a creation
    
    solids[state_id] = MVSDGroup(particle_ids, type, state_id);
    solids[state_id].assign_center(sumx / n, sumy / n);
}
};
