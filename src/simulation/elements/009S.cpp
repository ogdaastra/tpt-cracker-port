#include "simulation/ElementCommon.h"

int Element_009_update(UPDATE_FUNC_ARGS);
static int update(UPDATE_FUNC_ARGS);

void Element::Element_009S() {
	Identifier = "DEFAULT_PT_009S";
	Name = "009S";
	Colour = PIXPACK(0xFF3636);
	MenuVisible = 0;
	MenuSection = SC_SOLIDS;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = -0.0003f* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 100;

	DefaultProperties.temp = -R_TEMP + 50.0f + 273.15f;
	HeatConduct = 46;
	Description = "Solid SCP-009. Forms crystals.";

	Properties = TYPE_SOLID|PROP_LIFE_DEC|PROP_NEUTPASS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = 273.15f;
	LowTemperatureTransition = PT_009;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * - tmp:  vx
	 * - tmp2: vy
	 * - tmp4: crystal size
	 * - tmp3: Max crystal size
	 */
	if (parts[i].tmp3 == 0) {
		parts[i].tmp3 = RNG::Ref().between(1, 30);
		parts[i].tmp = RNG::Ref().chance(1, 3) ? 0 : RNG::Ref().chance(1, 2) ? -1 : 1;
		parts[i].tmp2 = RNG::Ref().chance(1, 3) ? 0 : RNG::Ref().chance(1, 2) ? -1 : 1;
	}

	// Crystal growth
	if (parts[i].tmp4 < parts[i].tmp3) {
		int r = pmap[y + parts[i].tmp2][x + parts[i].tmp];
		int rt = TYP(r);
		int ni = -1;

		if (rt == PT_PLNT || rt == PT_FLSH || rt == PT_UDDR || rt == PT_STMH) {
			sim->kill_part(ID(r));
			r = 0;
		}

		if (!r)
			ni = sim->create_part(-1, x + parts[i].tmp, y + parts[i].tmp2, PT_009S);
		else if (rt == PT_WATR || rt == PT_DSTW || rt == PT_SLTW || rt == PT_CBNW || rt == PT_IOSL || rt == PT_SWTR ||
				rt == PT_MILK || rt == PT_PULP || rt == PT_GLUE || rt == PT_WTRV || rt == PT_MUD || rt == PT_BCTR ||
				rt == PT_BLOD) {
			ni = ID(r);		
		}
		
		if (ni >= 0) {
			parts[ni].tmp = parts[i].tmp;
			parts[ni].tmp2 = parts[i].tmp2;
			parts[ni].tmp3 = parts[i].tmp3;
			parts[ni].tmp4 = parts[i].tmp4 + 1;
		}
	}

	return Element_009_update(sim, i, x, y, surround_space, nt, parts, pmap);
}
