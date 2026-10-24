#ifndef STASIS_H
#define STASIS_H
#include "Config.h"

class Simulation;

class Stasis {
public:
    Simulation &sim;

    //Arrays from the simulation    
    float vx[YRES / STASIS_CELL][XRES / STASIS_CELL];
    float vy[YRES / STASIS_CELL][XRES / STASIS_CELL];
    float strength[YRES / STASIS_CELL][XRES / STASIS_CELL];

    void Clear();
    Stasis(Simulation &sim);
};

#endif
