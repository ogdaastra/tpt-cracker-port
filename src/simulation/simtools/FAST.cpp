#include "simulation/ElementCommon.h"
#include "simulation/ToolCommon.h"

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);
void Element_CBMB_time_dilation(Simulation *sim, int x, int y, int radius, int val);

void SimTool::Tool_FAST() {
    Identifier = "DEFAULT_TOOL_FAST";
    Name = "FAST";
    Colour = PIXPACK(0xff7438);
    Description = "Speed up local time.";
    Perform = &perform;
}

static int perform(Simulation *sim, Particle *cpart, int x, int y, int brushX, int brushY, float strength) {
    Element_CBMB_time_dilation(sim, x, y, 1, strength * sim->MAX_TIME_DILATION);
    return 1;
}
