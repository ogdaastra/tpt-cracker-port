#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_SFLD() {
	Identifier = "DEFAULT_PT_SFLD";
	Name = "SFLD";
	Colour = PIXPACK(0x8EC6ED);
	MenuVisible = 1;
	MenuSection = SC_LIQUID;
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 1.00f;
	Loss = 1.0f;
	Collision = 1.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 30;

	DefaultProperties.temp = 20.0f;
	HeatConduct = 255;
	Description = "Superfluid neon. Likes to flow along surfaces.";

	Properties = TYPE_LIQUID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 27.102f;
	HighTemperatureTransition = PT_NEON;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, r;
	std::pair<int, int> newv;
	int max_solids_near = 0;

	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) {
					// Check if we can move to empty spot
					// Pick a spot to maximize number of nearby solids
					int s_count = 0;
					for (int rx2 = -1; rx2 <= 1; ++rx2)
					for (int ry2 = -1; ry2 <= 1; ++ry2)
					if (rx2 || ry2) {
						int r2 = pmap[y + ry + ry2][x + rx + rx2];
						if (r2 && (sim->elements[TYP(r2)].Properties & TYPE_SOLID || sim->elements[TYP(r2)].Properties & TYPE_PART)) {
							++s_count;
						}
					}
					if (s_count > max_solids_near) {
						max_solids_near = s_count;
						newv = std::make_pair(rx, ry);
					}
					continue;
				}
			}

	if (max_solids_near) {
		parts[i].vx = newv.first;
		parts[i].vy = newv.second;
	}

	return 0;
}
