#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_HOLY() {
	Identifier = "DEFAULT_PT_HOLY";
	Name = "HOLY";
	Colour = PIXPACK(0x5C91FF);
	MenuVisible = 1;
	MenuSection = SC_LIQUID; // SC_CRACKER2 in TPT Ultimata
	Enabled = 1;

	Advection = 0.2f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.2f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 30;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
	HeatConduct = 29;
	Description = "Holy Water. Cleanses sin.";

	Properties = TYPE_LIQUID | PROP_NEUTPASS | PROP_DEADLY;

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
	// Holy WATR kills:
	// JCB1, FIGH, STKM, STKM2, MONY, OIL, DEST, BOMB, SING, TANK
	// ACID, BCTR, VIRS, SPDR, Any fire type, EMP, WARP, GNSH

	// Cools down to room temperature if > room temp

	int rx, ry, r, rt;
	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) r = sim->photons[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				if (rt == PT_JCB1 || rt == PT_MONY || rt == PT_OIL  || rt == PT_DEST || rt == PT_BOMB ||
					rt == PT_SING || rt == PT_WARP || rt == PT_TANK || rt == PT_GNSH || rt == PT_EMP ||
					rt == PT_ACID || rt == PT_CAUS || rt == PT_VIRS || rt == PT_BCTR || rt == PT_SPDR ||
					rt == PT_CFLM || rt == PT_FIRE || rt == PT_PLSM || rt == PT_DFLM || rt == PT_FFLD ||
					rt == PT_SHLD4 || rt == PT_SHLD1 || rt == PT_SHLD2 || rt == PT_SHLD3 || rt == PT_LOVE ||
					rt == PT_NPLM || rt == PT_009 || rt == PT_009S || rt == PT_009G) {
					if (RNG::Ref().chance(1, 10) || rt == PT_MONY || rt == PT_LOVE || rt == PT_DFLM) {
						if (RNG::Ref().chance(9, 10))
							sim->kill_part(ID(r));
						else {
							sim->part_change_type(ID(r), x + rx, y + ry, PT_SMKE);
							parts[ID(r)].temp = 300.0f;
						}
					}
				}
				else if (parts[ID(r)].temp > 22.0f + 273.15f)
					parts[ID(r)].temp -= (parts[ID(r)].temp - (22.0f + 273.15f)) * 0.5f;
			}

	return 0;
}
