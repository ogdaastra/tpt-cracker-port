#ifndef GUNSHIP_H
#define GUNSHIP_H

#include "simulation/vehicles/vehicle.h"

extern Vehicle Gunship;
extern std::vector<VehiclePixel> GUNSHIP_BASE;
extern std::vector<VehiclePixel> THRUSTER_FRONT;
extern std::vector<VehiclePixel> THRUSTER_BACK;
void draw_gunship(Renderer *ren, Particle *cpart, float vx, float vy);

#endif