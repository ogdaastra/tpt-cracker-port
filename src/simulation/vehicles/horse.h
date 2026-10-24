#ifndef HORSE_H
#define HORSE_H

#include "simulation/ElementCommon.h"
#include "simulation/vehicles/vehicle.h"
#include <vector>

extern Vehicle Horse;
void draw_horse(Renderer *ren, Particle *cpart, float vx, float vy);

#endif