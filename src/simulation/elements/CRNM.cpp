#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_CRNM() {
	Identifier = "DEFAULT_PT_CRNM";
	Name = "CRNM";
	Colour = PIXPACK(0x7AAAD6);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.99f;
	Loss = 0.95f;
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

	DefaultProperties.temp = R_TEMP + 4.0f + 273.15f;
	HeatConduct = 251;
	Description = "Carolinium. Used to create long burning dark fire bombs. Explodes in layers.";

	Properties = TYPE_SOLID | PROP_NEUTPASS | PROP_RADIOACTIVE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15f + 3000.0f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, r;

	// PHOT -> NTRI
	r = sim->photons[y][x];
	if (TYP(r) == PT_PHOT)
		sim->part_change_type(ID(r), x, y, PT_NTRI);

	if (parts[i].life > 0 && sim->timer % 10 == 0) {
		sim->pv[y / CELL][x / CELL] += 20.0f;
		for (int rx = -1; rx <= 1; ++rx)
		for (int ry = -1; ry <= 1; ++ry)
			if (rx || ry) sim->create_part(-1, x + rx, y + ry, RNG::Ref().chance(1, 2) ? PT_PLSM : PT_DFLM);
		++parts[i].life;
	}
	if (parts[i].life > 10) {
		sim->kill_part(i);
		return 0;
	}

	// Nothing left to do
	if (parts[i].life)
		return 0;

	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;

				// Burn
				if ((sim->timer % 10 == 0 && TYP(r) == PT_CRNM && parts[ID(r)].life == 9) ||
						(TYP(r) == PT_FIRE || TYP(r) == PT_PLSM)) {
					parts[i].life = 1;
					return 0;
				}
			}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*pixel_mode |= PMODE_GLOW;

	if (cpart->life > 4) {
		*firea = 20;
		*firer = *colr, *fireg = *colg, *fireb = *colb;
		*pixel_mode |= FIRE_ADD;
	}

	float m = 1.1f - cpart->life / 10.0f;
	*colr *= m;
	*colg *= m;
	*colb *= m;

	return 0;
}
