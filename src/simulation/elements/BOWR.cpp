#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_BOWR() {
	Identifier = "DEFAULT_PT_BOWR";
	Name = "BOWR";
	Colour = PIXPACK(0xF5B400);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_CRACKER2 in TPT Ultimata
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.99f;
	Loss = 0.30f;
	Collision = -0.1f;
	Gravity = 2.0f;
	Diffusion = 0.75f;
	HotAir = 0.001f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 500;

	DefaultProperties.temp = 9999.0f;
	HeatConduct = 100;
	Description = "Bowserinator, highly destructive singularity";

	Properties = TYPE_GAS | PROP_NEUTPASS | PROP_INDESTRUCTIBLE;

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
	sim->gravmap[(y / CELL)*(XRES / CELL) + (x / CELL)] = 0.2f*(500);
	sim->create_part(-1, parts[i].x - 1, parts[i].y - 1, PT_THDR);
	sim->create_part(-1, parts[i].x - 1, parts[i].y + 1, PT_PLSM);
	sim->create_part(-1, parts[i].x + 1, parts[i].y + 1, PT_PLSM);
	sim->create_part(-1, parts[i].x + 1, parts[i].y - 1, PT_CO2);
	if (rand() % 200 == 0){
		sim->create_part(-1, parts[i].x - 1, parts[i].y - 1, PT_LIGH);
	}
	parts[i].temp = 999999.0f;

	int r, rx, ry, c = 0;
	for (rx = -1; rx < 2; rx++)
		for (ry = -1; ry < 2; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];

				// Kill even layered particles
				while (r && TYP(r) != PT_BOWR && c < 10) {
					sim->kill_part(ID(r));
					r = pmap[y + ry][x + rx];
					c++;
				}
			}
	return 0;
}
