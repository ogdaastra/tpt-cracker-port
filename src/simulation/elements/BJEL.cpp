#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_BJEL() {
	Identifier = "DEFAULT_PT_BJEL";
	Name = "BJEL";
	Colour = PIXPACK(0xBDB47D);
	MenuVisible = 1;
	MenuSection = SC_SENSOR;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 1.0f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 10;

	Weight = 100;

	HeatConduct = 2;
	Description = "Ballistic Jelly. Color changes from pressure.";

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	// Record max pressure magnitude
	int max = (int)fabs(sim->pv[y / CELL][x / CELL]);
	if (parts[i].tmp3 < max)
		parts[i].tmp3 = max;
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Brighten under high pressure
	float multi = 1.0f + (cpart->tmp3 / 256.0f) * 1.3f;
	*colr *= multi;
	*colg *= multi;
	*colb *= multi;
	return 0;
}
