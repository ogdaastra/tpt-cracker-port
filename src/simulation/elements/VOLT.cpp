#include "simulation/ElementCommon.h"
#include "simulation/magnetics/magnetics.h"
#include "simulation/circuits/framework.h"
#include "simulation/circuits/circuits.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_VOLT() {
	Identifier = "DEFAULT_PT_VOLT";
	Name = "VOLT";
	Colour = PIXPACK(0x826108);
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
	Hardness = 1;

	Weight = 100;

	HeatConduct = 251;
	Description = "Voltage source. Use PSCN/COPR and NSCN/ZINC for terminals. Set output voltage with tmp3.";

	Properties = TYPE_SOLID;
	DefaultProperties.tmp3 = 10.0f;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = ITH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 2000.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	CIRCUITS::add_circuit(x, y, sim);
	return 0;
}
