#include "simulation/ElementCommon.h"

int Element_009_update(UPDATE_FUNC_ARGS);
static int update(UPDATE_FUNC_ARGS);

void Element::Element_009G() {
	Identifier = "DEFAULT_PT_009G";
	Name = "009G";
	Colour = PIXPACK(0xFF3636);
	MenuVisible = 0;
	MenuSection = SC_GAS;
	Enabled = 1;

	Advection = 1.0f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.99f;
	Loss = 0.30f;
	Collision = -0.1f;
	Gravity = -0.1f;
	Diffusion = 0.75f;
	HotAir = 0.0003f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 4;

	Weight = 1;

	DefaultProperties.temp = -R_TEMP - 100.0f + 273.15f;
	HeatConduct = 48;
	Description = "Gaseous SCP-009.";

	Properties = TYPE_GAS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 171.0f;
	HighTemperatureTransition = PT_009;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	return Element_009_update(sim, i, x, y, surround_space, nt, parts, pmap);
}
