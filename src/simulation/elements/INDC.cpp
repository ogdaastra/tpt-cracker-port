#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_INDC() {
	Identifier = "DEFAULT_PT_INDC";
	Name = "INDC";
	Colour = PIXPACK(0xDF6742);
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
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 1;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 251;
	Description = "Inductor. Resists sudden changes in current. tmp3 = inductance.";
	DefaultProperties.tmp3 = 10.0f;
	DefaultProperties.tmp4 = 0.0f;

	Properties = TYPE_SOLID | PROP_CONDUCTS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 1085.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * tmp3 - inductance
	 * tmp4 - effective resistance
	 */

	return 0;
}
