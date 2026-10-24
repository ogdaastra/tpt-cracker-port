#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_PULP() {
	Identifier = "DEFAULT_PT_PULP";
	Name = "PULP";
	Colour = PIXPACK(0xEAEAEA);
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
	HotAir = 0.000f	* CFDS;
	Falldown = 2;

	Flammable = 50;
	Explosive = 0;
	Meltable = 0;
	Hardness = 50;

	Weight = 31;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
	DefaultProperties.life = 10000;

	HeatConduct = 164;
	Description = "Paper pulp. Slowly turns into paper.";
	Properties = TYPE_LIQUID | PROP_NEUTPENETRATE | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 232.8f + 273.15f; // 451 F
	HighTemperatureTransition = PT_FIRE;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	if (parts[i].life <= 0) {
		sim->part_change_type(i, x, y, PT_PAPR);
		parts[i].life = 0;
		return 1;
	}

	for (int rx = -1; rx <= 1; ++rx)
	for (int ry = -1; ry <= 1; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			int r = pmap[y + ry][x + rx];
			if (!r) continue;
			int rt = TYP(r);
			bool is_water = sim->elements[rt].Properties & PROP_WATER || rt == PT_WTRV;
		
			if (parts[i].life < 40000 && is_water) {
				parts[i].life += 100;
				sim->kill_part(ID(r));
			}
		}

	int life_dec_bonus = std::max(0, (int)((parts[i].temp - 273.15f) / 10.0f));
	parts[i].life -= life_dec_bonus;
	return 0;
}
