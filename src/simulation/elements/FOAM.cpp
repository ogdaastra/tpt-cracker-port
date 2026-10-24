#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_FOAM() {
	Identifier = "DEFAULT_PT_FOAM";
	Name = "FOAM";
	Colour = PIXPACK(0xFFFFFF);
	MenuVisible = 0;
	MenuSection = SC_SPECIAL;
	Enabled = 1;

	Advection = 0.7f;
	AirDrag = 0.02f * CFDS;
	AirLoss = 0.96f;
	Loss = 0.80f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 30;

	Weight = 18;

	HeatConduct = 70;
	Description = "Foam. Foamy.";
	DefaultProperties.life = 200;

	Properties = TYPE_PART | PROP_NEUTPASS | PROP_LIFE_DEC | PROP_LIFE_KILL;

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
	/**
	 * Properties:
	 * tmp: particles it can create
	 */
	if (fabs(sim->pv[y / CELL][x / CELL]) > 5.0f || parts[i].temp < 273.15f || parts[i].temp > 373.15f) {
		sim->kill_part(i);
		return 1;
	}

	if (parts[i].tmp > 0 && RNG::Ref().chance(1, 10)) {
		int rx, ry, r;
		for (rx = -1; rx <= 1; rx++)
		for (ry = -1; ry <= 1; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				if (parts[i].tmp <= 0) return 0;

				r = pmap[y + ry][x + rx];
				if (!r) {
					int ni = sim->create_part(-1, x + rx, y + ry, PT_FOAM);
					if (ni >= 0) {
						parts[ni].tmp = parts[i].tmp - 1;
						parts[i].tmp--;
					}
					continue;
				}

				if (TYP(r) == PT_GEL || TYP(r) == PT_SPNG) {
					sim->kill_part(i);
					return 1;
				}
			}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*pixel_mode |= PMODE_BLUR;

	*pixel_mode |= FIRE_BLEND;
	*firer = *fireg = *fireb = 100;
	*firea = 20;

	return 0;
}
