#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_GLUE() {
	Identifier = "DEFAULT_PT_GLUE";
	Name = "GLUE";
	Colour = PIXPACK(0xFAF3DC);
	MenuVisible = 1;
	MenuSection = SC_LIQUID;
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f  * CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 100;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
	HeatConduct = 29;
	Description = "Glue. Use it to stick powders together into solids. Highly flammable.";

	Properties = TYPE_LIQUID|PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15f + 300.0f;
	HighTemperatureTransition = PT_CRMC;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int r, rx, ry, rt;
	for (rx=-2; rx<3; rx++)
		for (ry=-2; ry<3; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y+ry][x+rx];
				if (!r) continue;
				rt = TYP(r);

				// Negate any powder velocities nearby
				if (sim->elements[rt].Properties & TYPE_PART) {
					parts[ID(r)].vx = parts[ID(r)].vy = 0;
					if (RNG::Ref().chance(1, 50) == 0) // Randomly "harden"
						parts[i].tmp = 1;
				}

				// Stickness
				if (sim->timer % 3 == 0) {
					parts[i].vx += rx / 10.0f;
					parts[i].vy += ry / 10.0f;
					sim->pv[y / CELL][x / CELL] -= 0.0002f;
				}

				// Glue "hardens" under pressure
				if (sim->pv[y / CELL][x / CELL] > 10.0f)
					parts[i].tmp = 0;

				// Glue "melts" when heated
				if (parts[i].temp > 273.15 + 70.0f)
					parts[i].tmp = 0;

				// "Hardened"
				if (parts[i].tmp)
					parts[i].vx = parts[i].vy = 0;

				// Reactions
				switch(rt) {
					case PT_GLOW: {
						sim->part_change_type(ID(r), x, y, PT_LCRY);
						break;
					}
					case PT_FIRE: {
						parts[i].life = RNG::Ref().between(0, 100) + 100;
						sim->part_change_type(i, x, y, PT_FIRE);
						break;
					}
				}
			}

	return 0;
}
