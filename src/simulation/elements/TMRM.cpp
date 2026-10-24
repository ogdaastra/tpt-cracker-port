#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_TMRM() {
	Identifier = "DEFAULT_PT_TMRM";
	Name = "TMRM";
	Colour = PIXPACK(0xB35539);
	MenuVisible = 1;
	MenuSection = SC_CIRCUITS; // SC_ELECTROMAG in TPT Ultimata; SC_ELEC before the menu split
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 1;
	Hardness = 10;
	Weight = 100;

	HeatConduct = 150;
	Description = "Thermium. Conductor, only melts under exposure to lightning.";

	Properties = TYPE_SOLID | PROP_CONDUCTS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15f + 4000.0;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	for (int rx = -1; rx <= 1; rx++)
	for (int ry = -1; ry <= 1; ry++)
		if (BOUNDS_CHECK && (rx || ry)) {
			int r = sim->pmap[y + ry][x + rx];
			if (!r) continue;

			if (TYP(r) == PT_LIGH && parts[ID(r)].temp > 5000.0 + 273.15f) {
				parts[i].temp += 1000.0f;
				parts[i].ctype = PT_TMRM;
				sim->part_change_type(i, x, y, PT_LAVA);
				return 1;
			}
		}

	return 0;
}
