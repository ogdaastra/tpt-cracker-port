#ifndef CIRCUITS_RESISTANCE
#define CIRCUITS_RESISTANCE

#include "simulation/ElementCommon.h"
#include "circuit_core.h"

bool valid_conductor(ElementType typ, Simulation *sim, ParticleId i);
Ohms get_resistance(ElementType type, Particle *parts, ParticleId i, Simulation *sim);
Ohms get_effective_resistance(ElementType type, Particle *parts, ParticleId i, Simulation *sim);

#endif