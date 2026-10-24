#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_RSTR() {
	Identifier = "DEFAULT_PT_RSTR";
	Name = "RSTR";
	Colour = PIXPACK(0xEACE9E);
	MenuVisible = 1;
	MenuSection = SC_ELEC; // SC_ELECTROMAG in TPT Ultimata
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
	Description = "Resistor. Set resistivity / px as tmp3.";

	Properties = TYPE_SOLID | PROP_CONDUCTS | PROP_LIFE_DEC;
	DefaultProperties.tmp3 = 1000;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 1400.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	// tmp3 = restivity
	if (parts[i].tmp3 <= 0.0f)
		parts[i].tmp3 = 1.0f;
	return 0;
}
