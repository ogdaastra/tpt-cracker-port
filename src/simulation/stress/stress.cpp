#include "stress.h"
#include "simulation/Simulation.h"
#include "simulation/ElementCommon.h"
#include <cmath>
#include <iostream>

#define PI_4 3.141592f / 4.0f

StressField::StressField(Simulation * simulation) : sim(simulation) {
    
}

void StressField::Clear() {
    if (!enabled) return;
    std::fill(&stress_map[0][0], &stress_map[YRES][0], 0.0f);
    std::fill(&stress_map_x[0][0], &stress_map_x[YRES][0], 0.0f);
    std::fill(&stress_map_y[0][0], &stress_map_y[YRES][0], 0.0f);
    std::fill(&gravitational_stress_map[0][0], &gravitational_stress_map[YRES / CELL][0], 0.0f);
}


void StressField::ComputeStress(int x, int y) {
    if (!enabled) return;

    float fx = 0.0f, fy = 0.0f;
    int r = sim->pmap[y][x];

    if (!r) return;

    // if (elements[parts[i].type].Properties & TYPE_SOLID)

    // Stress from gravity and newtonian gravity
    // TODO precompute effective weights or something from broken and shit
    sim->GetGravityField(x, y, sim->elements[TYP(r)].Weight / 10.0f, sim->elements[TYP(r)].Weight / 10.0f, fx, fy);

    // Stress from stasis field
    fx += (sim->stasis->vx[y / STASIS_CELL][x / STASIS_CELL] - sim->parts[ID(r)].vx) * sim->stasis->strength[y / STASIS_CELL][x / STASIS_CELL];
    fy += (sim->stasis->vy[y / STASIS_CELL][x / STASIS_CELL] - sim->parts[ID(r)].vy) * sim->stasis->strength[y / STASIS_CELL][x / STASIS_CELL];

    // Stress from pressure

    // Stress from particle collisions

    // Distribute stress
    // Compute which cells the angle will intersect and do a weighted distribution
    // Only distribute to cells with a particle, and not away from direction of stress



    float force_angle = atan2(fy, fx);
    float stress = fabs(sqrtf(fy * fy + fx * fx));
    float sub_stress = stress / 5.0f * 0.5f;

    int compression_particles = 1;
    int tension_particles = 0;
    int total_particles = 1;
    bool has_tension = sim->elements[TYP(r)].Properties & TYPE_SOLID;

    // The 3 pixels roughly in the direction of the force
    int nox1 = isign((float)round(cos(force_angle))), noy1 = isign((float)round(sin(force_angle)));
    int nox2 = isign((float)round(cos(force_angle + PI_4))), noy2 = isign((float)round(sin(force_angle + PI_4)));
    int nox3 = isign((float)round(cos(force_angle - PI_4))), noy3 = isign((float)round(sin(force_angle - PI_4)));

    // Compression, apply in direction
    for (int rx = -1; rx <= 1; ++rx)
    for (int ry = -1; ry <= 1; ++ry)
        if (rx || ry) {
            if (!sim->pmap[y + ry][x + rx] && !sim->IsWallBlocking(x + rx, y + ry, PT_DUST))
                continue;
            if ((rx == nox1 && ry == noy1) || (rx == nox2 && ry == noy2) || (rx == nox3 && ry == noy3)) {
                compression_particles += 2;
                // if (has_tension)
                stress -= sub_stress;
            }
            else if (has_tension) {
                tension_particles++;  
            }
        }

    if (tension_particles <= 4 && has_tension)
        total_particles = tension_particles + compression_particles;
    else
        total_particles = compression_particles;

    for (int rx = -1; rx <= 1; ++rx)
    for (int ry = -1; ry <= 1; ++ry)
        if (rx || ry) {
            if (!sim->pmap[y + ry][x + rx] && !sim->IsWallBlocking(x + rx, y + ry, PT_DUST))
                continue;
            if ((rx == nox1 && ry == noy1) || (rx == nox2 && ry == noy2) || (rx == nox3 && ry == noy3)) {
                stress_map_x[y + ry][x + rx] += 2 * (stress * cos(atan2(ry, rx)) + stress_map_x[y][x]) / total_particles;
                stress_map_y[y + ry][x + rx] += 2 * (stress * sin(atan2(ry, rx)) + stress_map_y[y][x]) / total_particles;
            }
            else if (tension_particles <= 4 && has_tension) {
                stress_map_x[y + ry][x + rx] += (stress * cos(atan2(ry-isign(fy), rx-isign(fx))) + stress_map_x[y][x]) / total_particles;
                stress_map_y[y + ry][x + rx] += (stress * sin(atan2(ry-isign(fy), rx-isign(fx))) + stress_map_y[y][x]) / total_particles;
            }
        }
    stress_map_x[y][x] += stress / total_particles * cos(force_angle);
    stress_map_y[y][x] += stress / total_particles * sin(force_angle);
}

void StressField::AggregateStress(int start, int end, int parts_lastActiveIndex) {
    if (!enabled) return;

    int x, y;
    for (int i = std::min(end, parts_lastActiveIndex); i >= start; i--) {
		if (sim->parts[i].type) {
            x = (int)(sim->parts[i].x + 0.5f);
            y = (int)(sim->parts[i].y + 0.5f);
            ComputeStress(x, y);
        }
    }

    for (int x = 0; x < XRES; x++)
    for (int y = 0; y < YRES; y++) {
        if (!stress_map_x[y][x] && !stress_map_y[y][x])
            continue;
        if (!sim->pmap[y][x])
            continue;
        stress_map[y][x] = sqrtf(stress_map_x[y][x]*stress_map_x[y][x]+stress_map_y[y][x]*stress_map_y[y][x]);

        // TRUS reduces stress
        if (TYP(sim->pmap[y][x]) == PT_TRUS)
            stress_map[y][x] *= 0.9f;

        for (int rx = -1; rx <= 1; ++rx)
        for (int ry = -1; ry <= 1; ++ry) {
            stress_map[y][x] -= (stress_map[y][x] - stress_map[y + ry][x + rx]) * 0.5f;
        }

        //if (stress_map[y][x] > 55.0f && TYP(sim->pmap[y][x]) != PT_DMND)
        //    sim->part_change_type(ID(sim->pmap[y][x]), x, y, PT_BRMT);
    }
}
