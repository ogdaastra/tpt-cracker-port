#ifndef KV2_H
#define KV2_H

#include "simulation/vehicles/vehicle.h"

extern Vehicle KV2;
extern std::vector<VehiclePixel> KV2_PIXELS;
void draw_kv2(Renderer *ren, Particle *cpart, float rotation);

#endif