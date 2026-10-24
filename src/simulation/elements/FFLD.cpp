#include "simulation/ElementCommon.h"
#include "simulation/Air.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_FFLD() {
	Identifier = "DEFAULT_PT_FFLD";
	Name = "FFLD";
	Colour = PIXPACK(0xDFEEF5);
	MenuVisible = 0;
	MenuSection = SC_EXPLOSIVE;
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

	DefaultProperties.temp = R_TEMP + 0.0f + 273.15f;
	DefaultProperties.life = 0;

	HeatConduct = 88;
	Description = "Force field. Hidden element.";

	Properties = TYPE_ENERGY | PROP_LIFE_DEC | PROP_LIFE_KILL | PROP_NOAMBHEAT | PROP_NOCTYPEDRAW;

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
	 * ctype: type to affect
	 * tmp2: type
	 *   0: blocks all
	 *   1: delete ctype (or all)
	 *   2: delete all but ctype
	 *   3: allow only ctype to pass
	 *   4: repel with gravity
	 *   5: Superheat
	 *   6: Supercool
	 *   7: Stasis field
	 */
	if (parts[i].tmp2 == 4) // Repel with gravity
		sim->gravmap[(y / CELL) * (XRES / CELL) + (x / CELL)] = -3.0f;
	else // Negate local gravity
		sim->gravmap[(y / CELL) * (XRES / CELL) + (x / CELL)] = 0;
	parts[i].vx = parts[i].vy = 0;

	int r, rx, ry;
	for (rx = -3; rx < 4; rx++)
		for (ry = -3; ry < 4; ry++)
			if (BOUNDS_CHECK) {
				r = pmap[y+ry][x+rx];
				if (!r) r = sim->photons[y + ry][x + rx];
				if (!r || TYP(r) == PT_FFLD || TYP(r) == PT_FFGN) continue;

				// Block LIGH (also in LIGH.cpp)
				if (TYP(r) == PT_LIGH) {
					sim->kill_part(ID(r));
					continue;
				}

				// Cure VIRS
				if (TYP(r) == PT_VIRS || TYP(r) == PT_VRSG || TYP(r) == PT_VRSS)
					parts[ID(r)].tmp3 = 8;

				if (parts[i].tmp2 == 0) { // Block all
					parts[ID(r)].vx = parts[ID(r)].x - parts[i].x;
					parts[ID(r)].vy = parts[ID(r)].y - parts[i].y;
				}
				else if (parts[i].tmp2 == 1) { // Delete ctype (or all if no ctype)
					if (!parts[i].ctype || TYP(r) == parts[i].ctype)
						sim->kill_part(ID(r));
				}
				else if (parts[i].tmp2 == 2) { // Delete all but ctype
					if (!parts[i].ctype || TYP(r) != parts[i].ctype)
						sim->kill_part(ID(r));
				}
				else if (parts[i].tmp2 == 3) { // Only ctype can pass
					if (TYP(r) != parts[i].ctype) {
						parts[ID(r)].vx = parts[ID(r)].x - parts[i].x;
						parts[ID(r)].vy = parts[ID(r)].y - parts[i].y;
					}
				}
				else if (parts[i].tmp2 == 5) { // Superheat
					parts[ID(r)].temp = 9999.0f;
				}
				else if (parts[i].tmp2 == 6) { // Supercool
					parts[ID(r)].temp = -273.15f;
				}
				else if (parts[i].tmp2 == 7) { // Stasis field, useful for negating energy particles
					parts[ID(r)].vx = 0;
					parts[ID(r)].vy = 0;
				}
			}

	sim->air->bmap_blockair[y/CELL][x/CELL] = 1;
	sim->air->bmap_blockairh[y/CELL][x/CELL] = 0x8;

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Force fields are a random blue-white glow
	int value = RNG::Ref().between(0, 100) + 90;
	*colr = value;
	*colg = value;
	*colb = RNG::Ref().between(0, 55) + 200;

	*firea = 55;
	*firer = *colr;
	*fireg = *colg;
	*fireb = *colb;

	*pixel_mode = PMODE_GLOW;
	*pixel_mode |= FIRE_ADD;

	return 0;
}
