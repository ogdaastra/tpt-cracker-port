#ifndef STASIS_H
#define STASIS_H
#include "Config.h"
#include "simulation/WorldGrid.h"

class Simulation;

class Stasis {
public:
    Simulation &sim;

    //Arrays from the simulation    
    WorldGrid<float> vx;
    WorldGrid<float> vy;
    WorldGrid<float> strength;

    void Clear();
    Stasis(Simulation &sim);
};

#endif
