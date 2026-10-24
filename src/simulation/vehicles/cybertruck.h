#ifndef CYBERTRUCK_H
#define CYBERTRUCK_H

#include "simulation/vehicles/vehicle.h"

extern Vehicle Cybertruck;
extern std::vector<VehiclePixel> CYBERTRUCK_PIXELS;
void draw_cybertruck(Renderer *ren, Particle *cpart, float rotation);

#endif