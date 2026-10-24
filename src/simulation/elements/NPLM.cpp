#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_NPLM() {
	Identifier = "DEFAULT_PT_NPLM";
	Name = "NPLM";
	Colour = PIXPACK(0xB00000);  // Get it BOOOO
	MenuVisible = 1;
	MenuSection = SC_EXPLOSIVE;
	Enabled = 1;
	
	Advection = 0.5f;
	AirDrag = 0.02f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.97f;
	Collision = 0.0f;
	Gravity = 0.2f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 2;
	
	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;
	
	Weight = 24;
	
	DefaultProperties.temp = R_TEMP + 273.15f;
	DefaultProperties.life = 1200;

	HeatConduct = 88;
	Description = "Napalm. Ultra-long burning explosive.";
	
	Properties = TYPE_LIQUID | PROP_LIFE_KILL;
	
	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 4000.0f;
	HighTemperatureTransition = PT_H2;
	
	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	if (parts[i].tmp > 0 && sim->timer % 10 == 0) {
		int j = sim->create_part(-1, x - 1, y - 1, PT_FIRE);
		if (j > 0) parts[j].life = RNG::Ref().between(1, 200);
		
		for (int k = -1; k <= 1; k += 2) {
			j = sim->create_part(-1, x + k, y + k, PT_BCOL);
			if (j > 0) {
				parts[j].life = RNG::Ref().between(0, 400);
				parts[j].vx = RNG::Ref().between(-15, 15);
				parts[j].vy = RNG::Ref().between(-15, 15);
			}
		}
	}

	// Fix burning forever
	if (RNG::Ref().chance(1, 100))
		parts[i].life--;

	int r, rx, ry;
	for (rx = -1; rx < 2; rx++) {
		for (ry = -1; ry < 2; ry++) {
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r || TYP(r) == PT_NPLM)
					continue;

				if (sim->elements[TYP(r)].Properties & TYPE_PART ||
					sim->elements[TYP(r)].Properties & TYPE_SOLID)
					parts[i].vx = parts[i].vy = 0;

				if (TYP(r) == PT_FIRE || TYP(r) == PT_PLSM || parts[ID(r)].type == PT_LAVA || parts[i].temp >= 1273.15f) {
					parts[i].tmp = 1;
					parts[ID(r)].vx *= 2;
					parts[ID(r)].vy *= 2;
					parts[ID(r)].life++;
					parts[i].temp += 50.0f;
					parts[i].life--;
				}
			}
		}
	}
	return 0;
}
