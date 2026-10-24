#include "simulation/ToolCommon.h"
#include "simulation/ElementCommon.h"

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_ERODE() {
	Identifier = "DEFAULT_TOOL_EROD";
	Name = "EROD";
	Colour = PIXPACK(0xcc361f);
	Description = "Erode the edges of solids and powders.";
    Perform = &perform;
}

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength) {
	if (!cpart || !(sim->elements[cpart->type].Properties & TYPE_PART || sim->elements[cpart->type].Properties & TYPE_SOLID))
		return 0;
	
    bool has_edge = false;
    for (int rx = -1; rx <= 1; ++rx)
    for (int ry = -1; ry <= 1; ++ry)
        if (BOUNDS_CHECK && (rx || ry)) {
            if (!sim->pmap[y + ry][x + rx]) {
                has_edge = true;
                goto end;
            }
        }
    end:
    if (!has_edge)
        return 0;

    if (RNG::Ref().chance(1, 100))
        sim->kill_part(ID(sim->pmap[y][x]));
	return 1;
}
