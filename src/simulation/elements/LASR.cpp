#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_LASR() {
	Identifier = "DEFAULT_PT_LASR";
	Name = "LASR";
	Colour = PIXPACK(0xFF0000);
	MenuVisible = 0;
	MenuSection = SC_SPECIAL;
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
	Meltable = 0;
	Hardness = 0;

	Weight = 100;

	DefaultProperties.temp = 9999.0f;
	DefaultProperties.life = 2;

	HeatConduct = 255;
	Description = "Laser. Hidden element.";

	Properties = TYPE_ENERGY | PROP_DEADLY | PROP_LIFE_DEC | PROP_LIFE_KILL | PROP_NOCTYPEDRAW;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	parts[i].temp = MAX_TEMP;
	parts[i].tmp = 0;

	int rx, ry, rt, r;
	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);
				parts[i].tmp = 1;

				// Instantly heat non-insulators
				if (sim->elements[rt].HeatConduct)
					parts[ID(r)].temp = MAX_TEMP;

				// Chance to kill non-indestructible elements
				if (!(sim->elements[rt].Properties & PROP_INDESTRUCTIBLE) && rt != PT_GNSH && rt != PT_BOMB && rt != PT_EMBR &&
						rt != PT_PCLN && rt != PT_BCLN && rt != PT_VIBR) {
					if (RNG::Ref().chance(1, 5))
						sim->kill_part(ID(r));
				}

				// Ignite flammable materials
				if (sim->elements[rt].Flammable)
					sim->part_change_type(ID(r), x + rx, y + ry, PT_FIRE);
			}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*firer = 255; *fireg = 0; *fireb = 0; *firea = 35;
	*colr = 255; *colg = 100; *colb = 100;
	*pixel_mode |= FIRE_ADD;

	/* Flare if hit a target */
	if (cpart->tmp) {
		*firer = 255; *fireg = 255; *fireb = 255; *firea = 255;
		*colg = 255; *colb = 255;
		*pixel_mode |= PMODE_ADD;
		*pixel_mode |= PMODE_LFLARE;
	}
	return 0;
}
