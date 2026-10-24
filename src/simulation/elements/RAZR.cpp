// RAZR from Jacob1's Mod (github.com/jacob1/The-Powder-Toy, GPL-3.0): very heavy powder that sinks
// through everything and kills stickmen.
#include "simulation/ElementCommon.h"

void Element::Element_RAZR()
{
	Identifier = "DEFAULT_PT_RAZR";
	Name = "RAZR";
	Colour = PIXPACK(0xC0C0C0);
	MenuVisible = 1;
	MenuSection = SC_POWDERS;
	Enabled = 1;

	Advection = 0.7f;
	AirDrag = 0.07f * CFDS;
	AirLoss = 0.97f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 1.5f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 500;

	HeatConduct = 50;
	Description = "Heavy silver particles, displaces everything. (Jacob1's Mod)";

	Properties = TYPE_PART|PROP_DEADLY;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;
}
