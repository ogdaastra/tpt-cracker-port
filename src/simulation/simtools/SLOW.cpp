#include "simulation/ElementCommon.h"
#include "simulation/ToolCommon.h"

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);
void Element_CBMB_time_dilation(Simulation *sim, int x, int y, int radius, int val);

void SimTool::Tool_SLOW() {
    Identifier = "DEFAULT_TOOL_SLOW";
    Name = "SLOW";
    Colour = PIXPACK(0x73e8fa);
    Description = "Slow down local time.";
    Perform = &perform;
}

static int perform(Simulation *sim, Particle *cpart, int x, int y, int brushX, int brushY, float strength) {
    Element_CBMB_time_dilation(sim, x, y, 1, strength * sim->MIN_TIME_DILATION);
    return 1;
}
