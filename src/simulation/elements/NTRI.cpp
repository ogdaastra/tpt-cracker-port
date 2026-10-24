#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
void Element_NEUT_create(ELEMENT_CREATE_FUNC_ARGS);
static int DeutExplosion(Simulation *sim, int n, int x, int y, float temp, int t);

void Element::Element_NTRI() {
	Identifier = "DEFAULT_PT_NTRI";
	Name = "NTRI";
	Colour = PIXPACK(0xEB34C3);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 1.00f;
	Loss = 1.00f;
	Collision = -0.99f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = -1;

	DefaultProperties.temp = R_TEMP + 900.0f + 273.15f;
	HeatConduct = 0;
	Description = "Neutrinos. Goes through everything, interacts with neutrons and electrons.";

	Properties = TYPE_ENERGY|PROP_LIFE_DEC|PROP_LIFE_KILL_DEC;

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
	Create = &Element_NEUT_create;
}

static int update(UPDATE_FUNC_ARGS) {
	int r = pmap[y][x];
	if (r) {
		int rt = TYP(r);
		
		// Send heat to HEAC
		if (rt == PT_HEAC)
			parts[ID(r)].temp = parts[i].temp;
		// Detonate DEUT
		else if (rt == PT_DEUT) {
			unsigned int pressureFactor = 3 + (int)sim->pv[y / CELL][x / CELL];
			if (RNG::Ref().chance(pressureFactor + 1 + (parts[ID(r)].life / 100), 1000)) {
				DeutExplosion(sim, parts[ID(r)].life, x, y,
					restrict_flt(parts[ID(r)].temp + parts[ID(r)].life * 500.0f, MIN_TEMP, MAX_TEMP), PT_NTRI);
				sim->kill_part(ID(r));
			}
		}
		// Copy heat from anything else
		else
			parts[i].temp = parts[ID(r)].temp;
	}

	r = sim->photons[y][x];
	if (r) {
		// NEUT -> ELEC + PHOT
		// ELEC -> PHOT
		if (TYP(r) == PT_NEUT) {
			sim->part_change_type(ID(r), x, y, PT_ELEC);
			int ni = sim->create_part(-3, x, y, PT_PHOT);
			if (ni > -1) parts[ni].temp = parts[i].temp;
		}
		else if (TYP(r) == PT_ELEC) {
			sim->part_change_type(ID(r), x, y, PT_PHOT);
		}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Glow if inside another element
	if (ren->sim->pmap[ny][nx]) {
		*firer = *colr;
		*fireg = *colg;
		*fireb = *colb;
		*firea = 255;
		*pixel_mode |= FIRE_ADD;
	}
	return 0;
}

static int DeutExplosion(Simulation * sim, int n, int x, int y, float temp, int t) {
	n = n / 50;
	if (n < 1)
		n = 1;
	else if (n > 340)
		n = 340;

	for (int c = 0; c < n; c++) {
		int i = sim->create_part(-3, x, y, t);
		if (i >= 0)
			sim->parts[i].temp = temp;
		else if (sim->pfree < 0)
			break;
	}
	sim->pv[y / CELL][x / CELL] += (6.0f * CFDS) * n;
	sim->gravmap[(y / CELL) * (XRES / CELL) + (x / CELL)] = 20.0f;
	return 0;
}
