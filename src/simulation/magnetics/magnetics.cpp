#include "magnetics.h"
#include "simulation/Simulation.h"
#include "simulation/ElementCommon.h"

#include <algorithm>
#include <iostream>

#define MAGNETIC_RADIUS_MULTIPLIER 2.0f
#define MAGNETIC_VELOCITY_MULTIPLIER 0.2f

// Helper
bool is_valid_loc(int x, int y) {
    return x >= 0 && x < XRES / EMCELL && y >= 0 && y < YRES / EMCELL;
}

// Actual magnetics
void EMField::Clear() {
    std::fill(&mx[0], &mx[0] + (EM_FIELD_SIZE), 0.0f);
    std::fill(&my[0], &my[0] + (EM_FIELD_SIZE), 0.0f);
    std::fill(&magnetic[0], &magnetic[0] + (EM_FIELD_SIZE), 0.0f);
    std::fill(&change_magnetic[0], &change_magnetic[0] + (EM_FIELD_SIZE), 0.0f);

    std::fill(&ex[0], &ex[0] + (EM_FIELD_SIZE), 0.0f);
    std::fill(&ey[0], &ey[0] + (EM_FIELD_SIZE), 0.0f);
    std::fill(&electric[0], &electric[0] + (EM_FIELD_SIZE), 0.0f);
    sources.clear();
}

EMField::EMField(Simulation &simulation) : sim(simulation) {
    Clear();
}

void EMField::PreUpdate() {
    if (!isEnabled) return;
    // if (sim.timer % 5 != 0) return;

    changed_this_frame = false;
    // added_sources.clear();
    // removed_sources.clear();

    // Record previous source array
    // std::copy(std::begin(sources), std::end(sources), std::begin(sources_cache));
    // std::fill(&sources[0], &sources[0] + (EM_FIELD_SIZE), 0.0f);
    std::copy(std::begin(magnetic), std::end(magnetic), std::begin(pmagnetic));
    
    // std::fill(&change_magnetic[0], &change_magnetic[0] + (EM_FIELD_SIZE), 0.0f);
}

int EMField::distance_2(int index1, int index2) {
    int x1 = index1 % (XRES / EMCELL), y1 = index1 / (XRES / EMCELL);
    int x2 = index2 % (XRES / EMCELL), y2 = index2 / (XRES / EMCELL);
    return (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2);
}

void EMField::ApplyElectromagneticForces(int i) {
    if (!isEnabled)
        return;

    int x = (int)(sim.parts[i].x + 0.5f) / EMCELL;
    int y = (int)(sim.parts[i].y + 0.5f) / EMCELL;
    int rt = sim.parts[i].type;

    if (sim.parts[i].type == PT_BRMT) {

        sim.parts[i].vx -= MAGNETIC_VELOCITY_MULTIPLIER * mx[FASTXY(x, y)];
        sim.parts[i].vy -= MAGNETIC_VELOCITY_MULTIPLIER * my[FASTXY(x, y)];

        sim.parts[i].vx += ex[FASTXY(x, y)];
        sim.parts[i].vy += ey[FASTXY(x, y)];
    }

    switch(rt) {
        // Negative charged
        case PT_ELEC:
        case PT_ION:
        case PT_BALI: {
            sim.parts[i].vx -= ex[FASTXY(x, y)] * 0.1f;
            sim.parts[i].vy -= ey[FASTXY(x, y)] * 0.1f;
            break;
        }
        // Positive charged
        case PT_PROT: {
            sim.parts[i].vx += ex[FASTXY(x, y)] * 0.1f;
            sim.parts[i].vy += ey[FASTXY(x, y)] * 0.1f;
            break;
        }
    }

    // else if (fabs(electric[FASTXY(x, y)]) > 0.1f && sim.elements[sim.parts[i].type].Properties & PROP_CONDUCTS) {
    //     if (!sim.photons[(int)sim.parts[i].y][(int)sim.parts[i].x])
    //         sim.create_part(-3, sim.parts[i].x, sim.parts[i].y, PT_RSPK);
    // }
}

void EMField::Update() {
    UpdateMagnetic();
    UpdateElectric();
}

void EMField::UpdateElectric() {
    if (!isEnabled) return;

    // Save original map
    std::copy(std::begin(electric), std::end(electric), std::begin(pelectric));
    std::copy(std::begin(ex), std::end(ex), std::begin(pex));
    std::copy(std::begin(ey), std::end(ey), std::begin(pey));


    // Advec electric velocity and pressure
    // for (int x = 0; x < XRES / EMCELL; ++x)
    // for (int y = 0; y < YRES / EMCELL; ++y) {
    //     int index1 = FASTXY(x, y);

    //     float tx = x + pex[index1] / EMCELL;
    //     float ty = y + pey[index1] / EMCELL;

    //     // Get the weighted locations of the four corners of the cell
    //     // the target is in, stored in these 2 temp arrays
    //     int cornerx[] = {(int)tx, (int)tx, (int)tx + 1, (int)tx + 1};
    //     int cornery[] = {(int)ty, (int)ty + 1, (int)ty, (int)ty + 1};

    //     for (unsigned int i = 0; i < 4; ++i) {
    //         int cx = cornerx[i], cy = cornery[i];
    //         if (cx >= 0 && cx < XRES / EMCELL && cy >= 0 && cy < YRES / EMCELL) {
    //             int dis = sqrtf((tx - cx) * (tx - cx) + (ty - cy) * (ty - cy));
    //             int index2 = FASTXY(cx, cy);
    //             electric[index2] += pelectric[index1] * (1 - dis / 1.414) * 1.0f;
    //             ex[index2] += pex[index1] * (tx - cx) / pow(dis, 2.0); // * (1 - dis / 1.414);
    //             ey[index2] += pey[index1] * (ty - cy) / pow(dis, 2.0); // * (1 - dis / 1.414);
    //         }
    //     }

    //     electric[index1] -= pelectric[index1];
    //     ex[index1] -= pex[index1];
    //     ey[index1] -= pey[index1];
    // }

    // Calculate new velocity map from pressure
    for (int x = 0; x < XRES / EMCELL; ++x)
    for (int y = 0; y < YRES / EMCELL; ++y) {
        float netx = 0.0f, nety = 0.0f, net = 0.0f;
        for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy) {
            if (!(dx || dy))
                continue;
            int tx = x + dx;
            int ty = y + dy;
            int index2 = FASTXY(tx, ty);

            if (tx >= 0 && ty >= 0 && tx < XRES / EMCELL && ty < YRES / EMCELL) {
                net += 0.125f * pelectric[index2];
                netx += pelectric[index2] * dx;
                nety += pelectric[index2] * dy;
                
            }
            electric[index2] -= 0.125f * pelectric[index2];
        }
        ex[FASTXY(x, y)] += netx / 300.0f;
        ey[FASTXY(x, y)] += nety / 300.0f;
        electric[FASTXY(x, y)] += net;
        
    }

    for (int x = 0; x < XRES / EMCELL; ++x)
    for (int y = 0; y < YRES / EMCELL; ++y) {
        electric[FASTXY(x, y)] *= 0.9f;
        ey[FASTXY(x, y)] *= 0.9f;
        ex[FASTXY(x, y)] *= 0.9f;
    }
}

void EMField::UpdateMagnetic() {
    // if (!isEnabled) return;
    // if (sim.timer % 5 != 0) return;
    //if (!changed_this_frame)
    //    return;
    if (!isEnabled || !changed_this_frame)
        return;


    // Recompute all magnetic forces
    std::fill(&magnetic[0], &magnetic[0] + (EM_FIELD_SIZE), 0.0f);

    for (int x = 0; x < XRES / EMCELL; ++x)
    for (int y = 0; y < YRES / EMCELL; ++y) {
        if (change_magnetic[FASTXY(x, y)] == 0.0f)
            continue;
        
        for (int x2 = 0; x2 < XRES / EMCELL; ++x2)
        for (int y2 = 0; y2 < YRES / EMCELL; ++y2) {
            int dx = x - x2;
            int dy = y - y2;
            if (!(dx || dy))
                continue;
            magnetic[FASTXY(x2, y2)] += MAGNETIC_RADIUS_MULTIPLIER * change_magnetic[FASTXY(x, y)] / (dx*dx + dy*dy);
        }
    }
}

