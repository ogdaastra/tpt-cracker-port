#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int molten_graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_SUFR() {
	Identifier = "DEFAULT_PT_SUFR";
	Name = "SUFR";
	Colour = PIXPACK(0xDBD032);
	MenuVisible = 1;
	MenuSection = SC_POWDERS;
	Enabled = 1;

	Advection = 0.4f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 50;
	Explosive = 0;
	Meltable = 5;
	Hardness = 0;

	Weight = 60;
	HeatConduct = 150;
	Description = "Sulfur. Burns quickly into a blue flame.";

	Properties = TYPE_PART | PROP_DEADLY;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 115.2f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
	MoltenGraphics = &molten_graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, r, rt;

	for (rx = -1; rx < 2; ++rx)
	for (ry = -1; ry < 2; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (!r) continue;
			rt = TYP(r);

			// Blue fire
			if (rt == PT_FIRE) {
				parts[ID(r)].dcolour = 0xFF007BFF;
				parts[ID(r)].tmp2 = 1;
				parts[ID(r)].temp += 500.0f;
				sim->part_change_type(i, x, y, PT_CAUS);
				return 1;
			}

			else if (rt == PT_GLOW) {
				sim->kill_part(ID(r));
				sim->part_change_type(i, x, y, PT_ACID);
				return 1;
			}
			else if (rt == PT_BCOL) {
				sim->kill_part(ID(r));
				sim->part_change_type(i, x, y, PT_GUNP);
				return 1;
			}

			// Kill ANT
			else if (rt == PT_ANT && RNG::Ref().chance(1, 200))
				sim->part_change_type(ID(r), x + rx, y + ry, PT_DUST);
		}

	return 0;
}

static int molten_graphics(GRAPHICS_FUNC_ARGS) {
	*pixel_mode |= PMODE_BLUR;
	cpart->dcolour = 0x0;

	if (cpart->temp < 200.0f + 273.15f) {
		*colr = 156;
		*colg = *colb = 0;
	}
	else {
		*colr = cpart->life * 2 + 0xE0;
		*colg = cpart->life * 1 + 0x50;
		*colb = cpart->life / 2 + 0x10;

		if (*colr > 255) *colr = 255;
		if (*colg > 192) *colg = 192;
		if (*colb > 128) *colb = 128;

		*firea = std::min(cpart->temp / (273.15f + 700.0f), 1.0f) * 40;
		*firer = *colr;
		*fireg = *colg;
		*fireb = *colb;
		*pixel_mode |= FIRE_ADD;
	}
	return 0;
}
