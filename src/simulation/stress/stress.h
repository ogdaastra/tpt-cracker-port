#ifndef STRESS_H
#define STRESS_H

#include "Config.h"

class Simulation;

class StressField {
public:
    Simulation * sim;
    bool enabled = false;
    float stress_map[YRES][XRES];
    float stress_map_x[YRES][XRES];
    float stress_map_y[YRES][XRES];

    int gravitational_stress_map[YRES / CELL][XRES / CELL];

    const float max_stress = 2.0f;

    StressField(Simulation * sim);
    void AggregateStress(int start, int end, int parts_lastActiveIndex);
    void ComputeStress(int x, int y);
    void Clear();
private:
    
};

#endif