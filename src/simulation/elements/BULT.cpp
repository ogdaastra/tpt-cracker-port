#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_BULT() {
	Identifier = "DEFAULT_PT_BULT";
	Name = "BULT";
	Colour = PIXPACK(0xA9724B);
	MenuVisible = 1;
	MenuSection = SC_FORCE;
	Enabled = 1;

	Advection = 0.4f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 2;
	Hardness = 2;

	Weight = 90;

	HeatConduct = 255;
	Description = "Bullet. Fires when sparked in opposite direction of spark.";

	Properties = TYPE_PART | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * tmp:  target vx
	 * tmp2: target vy
	 */

	if (parts[i].tmp != 0 || parts[i].tmp2 != 0) {
		parts[i].vx = parts[i].tmp;
		parts[i].vy = parts[i].tmp2;
	}

	// Explode if too hot
	if (parts[i].temp > 1000.0f) {
		parts[i].temp += 4000;
		parts[i].life = 100;
		sim->pv[y/CELL][x/CELL] += 5;
		sim->part_change_type(i, x, y, PT_EMBR);
		return 1;
	}

	int rx, ry, r;
	for (rx = -1; rx <= 1; ++rx)
	for (ry = -1; ry <= 1; ++ry)
		if (BOUNDS_CHECK) {
			r = pmap[y + ry][x + rx];
			if (!r) continue;

			// Spark detection
			if (parts[i].tmp == 0 && parts[i].tmp2 == 0 && (rx || ry)) {
				if (TYP(r) == PT_SPRK) {
					parts[i].tmp = -rx * 10;
					parts[i].tmp2 = -ry * 10;
					return 1;
				}
			}
			// Collision
			else if ((parts[i].tmp || parts[i].tmp2) && rx == isign(parts[i].tmp) && ry == isign(parts[i].tmp2)) {
				parts[ID(r)].temp += 2500.0f;
				sim->pv[y / CELL][x / CELL] += 5.0f;
				sim->part_change_type(i, x, y, PT_EMBR);
				parts[i].life = 500;
				parts[i].temp += 300.0f;
				return 1;
			}
 		}

	return 0;
}
