#include "simulation/ElementCommon.h"

void Element::Element_TRST() {
	Identifier = "DEFAULT_PT_TRST";
	Name = "TRST";
	Colour = PIXPACK(0xE09972);
	MenuVisible = 1;
	MenuSection = SC_ELEC; // SC_ELECTROMAG in TPT Ultimata
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
	Description = "Thermoresistor, resistance is tmp3 + tmp4 * temp.";
	DefaultProperties.tmp3 = 1000.0f;
	DefaultProperties.tmp4 = 10.0f;

	Properties = TYPE_SOLID | PROP_CONDUCTS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 3000.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;
}
