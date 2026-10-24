#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_TPRS() {
	Identifier = "DEFAULT_PT_TPRS";
	Name = "TPRS";
	Colour = PIXPACK(0xBA002B);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL;
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
	Hardness = 1;
	
	Weight = 100;
	
	HeatConduct = 0;
	Description = "Tempreature Preserver. Keeps constant temp while transmitting heat";
	Properties = TYPE_SOLID;
	
	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPL;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITL;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int r, rx, ry;
	for (rx = -1; rx < 2; rx++)
		for (ry = -1; ry < 2; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r || TYP(r) == PT_TPRS) { r = sim->photons[y + ry][x + rx]; }
				if (!r) continue;

				float diffrence = (parts[i].temp - parts[ID(r)].temp) / 20.0f;
				parts[ID(r)].temp += diffrence;
				if (diffrence < 10.0f) {
					parts[ID(r)].temp = parts[i].temp;
				}
			}

	return 0;
}
