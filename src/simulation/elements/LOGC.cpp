#include "simulation/ElementCommon.h"
#include "simulation/circuits/framework.h"
#include "simulation/circuits/circuits.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_LOGC() {
	Identifier = "DEFAULT_PT_LOGC";
	Name = "LOGC";
	Colour = PIXPACK(0xFFFFFF);
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
	Description = "Multi-pixel Logic gate. Use PSCN / COPR for input, NSCN / ZINC for output. Tmp = type, tmp3 = output voltage.";
	DefaultProperties.tmp3 = 5.0f;

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 2300.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * tmp - Type
	 * 
	 * tmp2 - Positive inputs in this frame
	 * life - Negative inputs in this frame
	 * tmp3 - Output voltage
	 * tmp4 - Effective output voltage
	 */
	CIRCUITS::add_circuit(x, y, sim);
	
	int output_id = -1, ox, oy;
	for (int rx = -1; rx <= 1; rx++)
	for (int ry = -1; ry <= 1; ry++)
		if (BOUNDS_CHECK && (rx || ry)) {
			int r = pmap[y + ry][x + rx];
			if (!r) continue;

			if (TYP(r) == PT_INWR) {

			}
			
		}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// graphics code here
	// return 1 if nothing dymanic happens here

	return 1;
}
