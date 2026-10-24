#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
void Element_FIRE_create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_DFLM() {
	Identifier = "DEFAULT_PT_DFLM";
	Name = "DFLM";
	Colour = PIXPACK(0x3C115E);
	MenuVisible = 1;
	MenuSection = SC_EXPLOSIVE;
	Enabled = 1;

	Advection = 0.2f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.97f;
	Loss = 0.20f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 1.5f;
	HotAir = 0.001f  * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;
	Weight = 2;

	DefaultProperties.temp = R_TEMP + 900.0f + 273.15f;
	HeatConduct = 88;
	Description = "Dark fire. Slow burning but difficult to extinguish flames. Consumes all.";

	Properties = TYPE_GAS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = 10.0f;
	LowTemperatureTransition = PT_FIRE;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
	Graphics = &graphics;
	Create = &Element_FIRE_create;
}

static int update(UPDATE_FUNC_ARGS) {
	if (parts[i].life <= 0) { // "Renew" life
		if (parts[i].tmp) {   // No renew, just die
			sim->kill_part(i);
			return 0;
		}
		parts[i].life = RNG::Ref().between(10, 200);
		parts[i].temp += 400.0f;
	}

	int r, rx, ry, rt;
	bool seen_part = false;
	for (rx = -2; rx < 3; rx++)
		for (ry = -2; ry < 3; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if (sim->bmap[(y+ry)/CELL][(x+rx)/CELL] && sim->bmap[(y+ry)/CELL][(x+rx)/CELL]!=WL_STREAM)
					continue;
				rt = TYP(r);

				// Randomly "burn" particles
				if (!(sim->elements[rt].Properties & PROP_INDESTRUCTIBLE) && rt != PT_BCLN &&
					rt != PT_PCLN && rt != PT_VOID && rt != PT_PVOD &&
					rt != PT_BHOL && rt != PT_WHOL && rt != PT_NBHL && rt != PT_NWHL && rt != PT_CRNM &&
					rt != PT_FIRE && rt != PT_PLSM
						&& RNG::Ref().chance(1, 200)) {
					int ni = sim->part_change_type(ID(r), x + rx, y + ry, PT_DFLM);
					parts[ni].tmp = RNG::Ref().chance(1, 2); // Chance to become killable DFLM
				}

				// Kill nearby DFLM
				if (rt == PT_DFLM && RNG::Ref().chance(1, 3000))
					sim->kill_part(ID(r));

				if (rt != PT_DFLM)
					seen_part = true;
			}
	if (!seen_part && RNG::Ref().chance(1, 100))
		sim->kill_part(i);

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	float m = cpart->life / 200.0f;
	*firea = 95;
	*firer = *colr * m;
	*fireg = *colg * m;
	*fireb = *colb * m;

	*pixel_mode = PMODE_NONE; // Clear default, don't draw pixel
	*pixel_mode |= FIRE_ADD;
	return 0;
}
