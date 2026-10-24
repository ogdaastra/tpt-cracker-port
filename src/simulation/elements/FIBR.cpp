#include "simulation/ElementCommon.h"
#include <vector>

static int update(UPDATE_FUNC_ARGS);

const int MAX_PHOT_SPEED = 30;

void Element::Element_FIBR() {
	Identifier = "DEFAULT_PT_FIBR";
	Name = "FIBR";
	Colour = PIXPACK(0xA9C8AB);
	MenuVisible = 1;
	MenuSection = SC_ELEC;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 100;

	HeatConduct = 150;
	DefaultProperties.tmp = 30;
	Description = "Fiber optic cable. Moves photons, set tmp for transfer speed.";

	Properties = TYPE_SOLID | PROP_HOT_GLOW | PROP_SPARKSETTLE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = 5.0f;
	HighPressureTransition = PT_BGLA;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties
	 * - tmp: Speed
	 * 
	 * Increases life of PHOT inside
	 */

	// Melt into glass
	if (parts[i].temp > 1973.15f) {
		sim->part_change_type(i, x, y, PT_LAVA);
		parts[i].ctype = PT_GLAS;
		return 1;
	}

	// Limit max / min speeds
	if (parts[i].tmp < 0)
		parts[i].tmp = 0;
	if (parts[i].tmp > 50)
		parts[i].tmp = 50;

	// Keep photons alive
	int r = sim->photons[y][x];
	if (TYP(r) != PT_PHOT)
		return 0;

	parts[ID(r)].life++; // Keep phot alive
	return 0;
}
