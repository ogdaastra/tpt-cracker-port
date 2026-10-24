#include "simulation/ToolCommon.h"
#include "simulation/Air.h"

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_NORM() {
	Identifier = "DEFAULT_TOOL_NORM";
	Name = "NORM";
	Colour = PIXPACK(0x1aa336);
	Description = "Normalizes local temperature and pressure.";
	Perform = &perform;
}

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength) {
    sim->air->pv[y / CELL][x / CELL] -= sim->air->pv[y / CELL][x / CELL] * strength * 0.005f;
	if (!cpart)
		return 0;
	cpart->temp -= (cpart->temp - (273.15 + R_TEMP)) * 0.2f;
	return 1;
}
