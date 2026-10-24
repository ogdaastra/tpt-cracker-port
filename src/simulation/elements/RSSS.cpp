// From Cracker1000's Mod, newer than v55.1 (github.com/cracker1000/The-Powder-Toy, branch Bledge-2024),
// adapted to this TPT 97.0 code base. GPL-3.0.
#include "simulation/ElementCommon.h"
#include "simulation/Air.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_RSSS()
{
	Identifier = "DEFAULT_PT_RSSS";
	Name = "RSSS";
	Colour = PIXPACK(0xC43626);
	MenuVisible = 1;
	MenuSection = SC_SOLIDS;
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

	HeatConduct = 130;
	Description = "Solidified resist. Blocks pressure and insulates electricity. Liquefies on contact with neutrons.";

	Properties = TYPE_SOLID|PROP_NEUTPASS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS)
{
	//Block air like TTAN
	sim->air->bmap_blockair[y/CELL][x/CELL] = 1;
	sim->air->bmap_blockairh[y/CELL][x/CELL] = 0x8;

	return 0;
}
