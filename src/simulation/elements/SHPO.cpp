#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_SHPO() {
	Identifier = "DEFAULT_PT_SHPO";
	Name = "SHPO";
	Colour = PIXPACK(0xEAB5FF);
	MenuVisible = 1;
	MenuSection = SC_LIQUID; // SC_CRACKER2 in TPT Ultimata
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 35;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
	HeatConduct = 29;
	Description = "Shampoo. Creates foam, washes off deco color, and cures virus. Resets all properties, dissolves powders.";

	Properties = TYPE_LIQUID | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 373.15;
	HighTemperatureTransition = PT_WTRV;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * flags: number of empty spaces
	 */

	int rx, ry, r, rt;
	parts[i].flags = 0; // Track how many empty spaces
	for (rx = -2; rx <= 2; ++rx)
		for (ry = -2; ry <= 2; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r)
					r = sim->photons[y + ry][x + rx];
				if (!r) {
					parts[i].flags++;
					continue;
				}
				rt = TYP(r);

				if (rt == PT_SHPO) // Ignore shampoo
					continue;

				// Wash off deco color
				parts[ID(r)].dcolour = 0x0;

				// Reset properties to default
				// parts[ID(r)].temp = sim->elements[rt].DefaultProperties.temp;
				parts[ID(r)].tmp = sim->elements[rt].DefaultProperties.tmp;
				parts[ID(r)].tmp2 = sim->elements[rt].DefaultProperties.tmp2;
				parts[ID(r)].ctype = sim->elements[rt].DefaultProperties.ctype;
				parts[ID(r)].life = sim->elements[rt].DefaultProperties.life;
				parts[ID(r)].flags = sim->elements[rt].DefaultProperties.flags;
				parts[ID(r)].vx = sim->elements[rt].DefaultProperties.vx;
				parts[ID(r)].vy = sim->elements[rt].DefaultProperties.vy;
				if (sim->elements[rt].Create)
					sim->elements[rt].Create(sim, ID(r), x + rx, y + ry, rt, -1);

				// Dissolve powders
				if (sim->elements[rt].Properties & TYPE_PART && RNG::Ref().chance(1, 2000))
					sim->kill_part(ID(r));
			}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*pixel_mode |= PMODE_BLUR;

	// Bubbles
	if (cpart->flags > 2 && RNG::Ref().chance(1, 20)) {
		ren->fillcircle(cpart->x, cpart->y, 2, 2, 191, 228, 255, 255);
	}
	return 0;
}
