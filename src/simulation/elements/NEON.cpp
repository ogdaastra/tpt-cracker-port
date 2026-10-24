#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_NEON() {
	Identifier = "DEFAULT_PT_NEON";
	Name = "NEON";
	Colour = PIXPACK(0x031140);
	MenuVisible = 1;
	MenuSection = SC_GAS;
	Enabled = 1;

	Advection = 1.0f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.99f;
	Loss = 0.30f;
	Collision = -0.1f;
	Gravity = 0.0f;
	Diffusion = 0.75f;
	HotAir = 0.001f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;
	PhotonReflectWavelengths = 0x3FFF8000;

	Weight = 1;

	DefaultProperties.temp = R_TEMP + 2.0f + 273.15f;
	HeatConduct = 106;
	Description = "Neon Gas. Diffuses and conductive. Invisible until sparked, glows with dcolour.";

	Properties = TYPE_GAS | PROP_CONDUCTS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = 27.15f;
	LowTemperatureTransition = PT_SFLD;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	DefaultProperties.dcolour = 0xFFFF0000;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	// Fusion
	if (sim->pv[y / CELL][x / CELL] > 220.0f && parts[i].temp > 9273.15f) {
		sim->pv[y / CELL][x / CELL] += 30.0f;
		parts[i].temp = 9999.0f;
		sim->part_change_type(i, x, y, RNG::Ref().chance(1, 2) ? PT_O2 : PT_NBLE);
	}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Glow
	if (cpart->life) {
		*pixel_mode |= PMODE_GLOW | PMODE_BLUR;	
		*firer = *colr, *fireg = *colg, *fireb = *colb, *firea = cpart->life / 20.0f * 255;
		*cola = *firea;
	}
	// Inactive
	else {
		*pixel_mode |= NO_DECO;
	}
	return 0;
}
