#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_FREZ() {
	Identifier = "DEFAULT_PT_FREZ";
	Name = "FREZ";
	Colour = PIXPACK(0xDEF9FC);
	MenuVisible = 1;
	MenuSection = SC_GAS;
	Enabled = 1;

	Advection = 1.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.99f;
	Loss = 0.30f;
	Collision = -0.1f;
	Gravity = 0.5f;
	Diffusion = 1.0f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 1;

	DefaultProperties.temp = 0.0f;
	HeatConduct = 0;
	Description = "Frezon. (OP Feron) Super cold gas, does not gain heat energy";

	Properties = TYPE_GAS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = IPH;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int r, rx, ry;
	for (rx = -1; rx < 2; rx++)
		for (ry = -1; ry < 2; ry++)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (!r) r = sim->photons[y + ry][x + rx];
			if (!r) continue;

			if (TYP(r) != PT_FREZ && TYP(r) != PT_TPRS)
				parts[ID(r)].temp -= 25.0f;

			// Put out fires
			if (TYP(r) == PT_FIRE || TYP(r) == PT_PLSM || TYP(r) == PT_DFLM){
				sim->kill_part(ID(r));
				if (RNG::Ref().gen() % 100 == 0) {
					sim->kill_part(i);
					return 0;
				}
			}

			// Detonate C5
			if (TYP(r) == PT_C5) {
				parts[ID(r)].temp = 0.0f;
				parts[i].life = 100;
				sim->part_change_type(i, parts[i].x, parts[i].y, PT_CFLM);
			}
		}
		
	// FREZ fusion. Occurs at high pressures
	if (parts[i].temp < 100.0f && sim->pv[y / CELL][x / CELL] > 200.0f) {
		if (!(RNG::Ref().gen() % 5)) {
			int j;
			float temp = parts[i].temp;
			sim->create_part(i, x, y, PT_PLSM);
			parts[i].tmp = 0x1;

			j = sim->create_part(-3, x + RNG::Ref().gen() % 3 - 1, y + RNG::Ref().gen() % 3 - 1, PT_NEUT);
			if (j>-1)
				parts[j].temp = temp;
			if (!(RNG::Ref().gen() % 10)) {
				j = sim->create_part(-3, x + RNG::Ref().gen() % 3 - 1, y + RNG::Ref().gen() % 3 - 1, PT_ELEC);
				if (j>-1)
					parts[j].temp = temp;
			}
			j = sim->create_part(-3, x + RNG::Ref().gen() % 3 - 1, y + RNG::Ref().gen() % 3 - 1, PT_PHOT);
			if (j>-1) {
				parts[j].ctype = 0x7C0000;
				parts[j].temp = temp;
				parts[j].tmp = 0x1;
			}
			j = sim->create_part(-3, x + RNG::Ref().gen() % 3 - 1, y + RNG::Ref().gen() % 3 - 1, PT_PLSM);
			if (j>-1) {
				parts[j].temp = temp;
				parts[j].tmp |= 4;
			}
			parts[i].temp = temp + 750 + RNG::Ref().gen() % 500;
			sim->pv[y / CELL][x / CELL] += 30;
		}
	}
	return 0;
}
