#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_STAR() {
	Identifier = "DEFAULT_PT_STAR";
	Name = "STAR";
	Colour = PIXPACK(0xFFFF00);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_SPACE in TPT Ultimata
	Enabled = 1;

	Advection = 0.00f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.00f;
	Loss = 0.00f;
	Collision = 0.00f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f  * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 100;

	Weight = 200;

	DefaultProperties.temp = R_TEMP + 4.0f + 273.15f;
	
	HeatConduct = 10;
	Description = "Star, undergoes stellar evolution and provides light for planets.";

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	// Temp
	DefaultProperties.tmp = 9;
	DefaultProperties.life = 1150;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * tmp: star type
	 * 1. brown dwarf
	 * 2. red dwarf 
	 * 3. yellow sun
	 * 	> life = fuel remaining
	 * 4. red giant
	 * 5. red supergiant
	 * 6. blue supergiant
	 * 7. white dwarf
	 * 8. neutron star
	 * 9. black hole
	 * 	> life = rotational velocity
	 */
	if (parts[i].life > 0)
		parts[i].life--;

	int rx, ry, r;

	switch(parts[i].tmp) {
		case 9: { // Black hole
			sim->gravmap[(y / CELL) * (XRES / CELL) + (x / CELL)] += 0.2f;

			// TODO check bounds
			// TODO the constants like 1150 move somewhere
			// Rotational blackholes have slightly stronger gravity at poles
			float rotational_gravity_bonus = 0.2f * parts[i].life / 1150;
			sim->gravmap[(y / CELL) * (XRES / CELL) + ((x - 1) / CELL)] += rotational_gravity_bonus;
			sim->gravmap[(y / CELL) * (XRES / CELL) + ((x + 1) / CELL)] += rotational_gravity_bonus;

			// Kill nearby particles
			for (rx = -1; rx <= 1; ++rx)
			for (ry = -1; ry <= 1; ++ry)
				if (BOUNDS_CHECK) {
					r = pmap[y + ry][x + rx];
					if (!r) r = sim->photons[y + ry][x + rx];
					if (!r || TYP(r) == PT_STAR) continue;
					sim->kill_part(ID(r));
				}

			break;
		}
	}
	

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Is edge check
	int rx, ry, r;
	bool is_edge = false;
	for (rx = -1; rx <= 1; ++rx)
	for (ry = -1; ry <= 1; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = ren->sim->pmap[(int)(cpart->y + 0.5) + ry][(int)(cpart->x + 0.5) + rx];
			if (!r || TYP(r) != PT_STAR || ren->sim->parts[ID(r)].tmp != cpart->tmp) {
				is_edge = true;
				goto end;
			}
		}
	end:

	switch(cpart->tmp) {
		case 9: { // Black hole
			*colr = *colg = *colb = 0;
			if (is_edge) { // Outer blackhole glows
				*firer = 255; *fireg = 150;
				*firea = 120 - (ren->sim->timer % 30); // Flicker
				*firea *= (float)cpart->life / 1150;

				*pixel_mode |= FIRE_ADD;
				if (ren->sim->timer % 3 == 0 && cpart->life > 500) { // Border flicker
					*colr = 255; *colg = 150;
					*pixel_mode |= PMODE_FLARE;
				}
			}
			break;
		}
	}

	return 0;
}
