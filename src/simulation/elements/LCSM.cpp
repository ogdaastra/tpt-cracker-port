#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

int Element_CESM_update(UPDATE_FUNC_ARGS);
int Element_CESM_graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_LCSM() {
	Identifier = "DEFAULT_PT_LCSM";
	Name = "LCSM";
	Colour = PIXPACK(0xBED15E);
	MenuVisible = 1;
	MenuSection = SC_EXPLOSIVE;
	Enabled = 1;

	Advection = 0.3f;
	AirDrag = 0.02f * CFDS;
	AirLoss = 0.95f;
	Loss = 0.80f;
	Collision = 0.0f;
	Gravity = 0.15f;
	Diffusion = 0.00f;
	HotAir = 0.000001f* CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 50;
	Hardness = 1;

	Weight = 50;
	PhotonReflectWavelengths = 0x0F30000;

	HeatConduct = 200;
	Description = "Liquid cesium. Like rubidium but sexier.";

	Properties = TYPE_LIQUID | PROP_CONDUCTS | PROP_LIFE_DEC;
	DefaultProperties.temp = 273.15f + 36.0f;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = 28.0f + 273.15f;
	LowTemperature = PT_CESM;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15f + 1100.0f;
	HighTemperatureTransition = PT_FIRE;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	// No conducting if above 671 C
	// Also "shake"
	if (parts[i].temp > 671.0f + 273.15f) {
		parts[i].life = 4;
		parts[i].vx += RNG::Ref().uniform01() - 0.5f;
		parts[i].vy += RNG::Ref().uniform01() - 0.5f;
	}

	Element_CESM_update(sim, i, x, y, surround_space, nt, parts, pmap);
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*pixel_mode |= PMODE_BLUR;
	Element_CESM_graphics(ren, cpart, nx, ny, pixel_mode, cola, colr, colg, colb, firea, firer, fireg, fireb);

	// Glow when hot
	if (cpart->temp > 671.0f + 273.15f) {
		*pixel_mode |= FIRE_ADD;
		
		int strength = (cpart->temp - 671.0f - 273.15f) / 25.0f + 1;
		*firea = strength * 9;
		if (*firea > 255)
			*firea = 255;
		*firer = *fireg = *fireb = 255;
	}

	return 0;
}
