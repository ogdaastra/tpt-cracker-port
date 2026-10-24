#include "simulation/ElementCommon.h"

int Element_ACOL_update(UPDATE_FUNC_ARGS);
int Element_ACOL_graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_ACOL() {
	Identifier = "DEFAULT_PT_ACOL";
	Name = "ACOL";
	Colour = PIXPACK(0x222222);
	MenuVisible = 1;
	MenuSection = SC_SOLIDS;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.0f;
	HotAir = 0.0f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;
	PhotonReflectWavelengths = 0x00000000;

	Weight = 100;

	HeatConduct = 200;
	Description = "Anti-Coal, Burns very slowly with CFLM.";

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = 6.0f;
	HighPressureTransition = PT_BACL;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	DefaultProperties.life = 110;
	DefaultProperties.tmp = 50;

	Update = &Element_ACOL_update;
	Graphics = &Element_ACOL_graphics;
}

int Element_ACOL_update(UPDATE_FUNC_ARGS) {
	if (parts[i].life <= 0) {
		sim->create_part(i, x, y, PT_CFLM);
		return 1;
	} else if (parts[i].life < 100) {
		parts[i].life--;
		sim->create_part(-1, x + RNG::Ref().between(-1, 1), y + RNG::Ref().between(-1, 1), PT_CFLM);
		parts[i].temp -= 5.0f;
	}

	if (parts[i].life >= 100) {
		int rx, ry, r;
		for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (TYP(r) == PT_CFLM)
					parts[i].life--;
			}
	}
	return 0;
}

int Element_ACOL_graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->temp < 273.15f) {
		*colr += 273.15f - cpart->temp;
		*colg += 273.15f - cpart->temp;
		*colb += 273.15f - cpart->temp;
	}
	return 0;
}
