#include "Stasis.h"
#include "Simulation.h"

#include <algorithm>

void Stasis::Clear() {
	std::fill(&vy[0][0], &vy[0][0]+((XRES/STASIS_CELL)*(YRES/STASIS_CELL)), 0.0f);
	std::fill(&vx[0][0], &vx[0][0]+((XRES/STASIS_CELL)*(YRES/STASIS_CELL)), 0.0f);
	std::fill(&strength[0][0], &strength[0][0]+((XRES/STASIS_CELL)*(YRES/STASIS_CELL)), 0.0f);
}

Stasis::Stasis(Simulation & simulation): sim(simulation) {
	//Simulation should do this.
	std::fill(&vx[0][0], &vx[0][0]+((XRES/STASIS_CELL)*(YRES/STASIS_CELL)), 0.0f);
	std::fill(&vy[0][0], &vy[0][0]+((XRES/STASIS_CELL)*(YRES/STASIS_CELL)), 0.0f);
	std::fill(&strength[0][0], &strength[0][0] + ((XRES / STASIS_CELL) * (YRES / STASIS_CELL)), 0.0f);
}
