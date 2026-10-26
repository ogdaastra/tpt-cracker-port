#ifndef STRESS_H
#define STRESS_H

#include "Config.h"
#include "simulation/WorldGrid.h"

class Simulation;

class StressField {
public:
    Simulation * sim;
    bool enabled = false;
    WorldGrid<float> stress_map;
    WorldGrid<float> stress_map_x;
    WorldGrid<float> stress_map_y;

    WorldGrid<int> gravitational_stress_map;

    const float max_stress = 2.0f;

    StressField(Simulation * sim);
    void AggregateStress(int start, int end, int parts_lastActiveIndex);
    void ComputeStress(int x, int y);
    void Clear();
private:
    
};

#endif