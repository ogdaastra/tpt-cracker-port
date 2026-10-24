#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_SWTR() {
	Identifier = "DEFAULT_PT_SWTR";
	Name = "SWTR";
	Colour = PIXPACK(0x5362F5);
	MenuVisible = 1;
	MenuSection = SC_ORGANIC; // SC_ORGANIC in TPT Ultimata; SC_LIQUID before the menu split
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

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 35;

	HeatConduct = 75;
	Description = "Sugar water.";

	Properties = TYPE_LIQUID | PROP_CONDUCTS | PROP_LIFE_DEC | PROP_NEUTPENETRATE | PROP_WATER;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = 252.05f;
	LowTemperatureTransition = PT_ICEI;
	HighTemperature = 383.0f;
	HighTemperatureTransition = ST;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int r, rx, ry;
	for (rx=-1; rx<2; rx++)
		for (ry=-1; ry<2; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y+ry][x+rx];
				switch TYP(r) {
				case PT_SUGR: // Grow sugar crystals
					if (RNG::Ref().chance(1, 2000))
						sim->part_change_type(i, parts[i].x, parts[i].y, PT_SUGR);
					break;
				case PT_PLNT:
					if (RNG::Ref().chance(1, 2000))
						sim->part_change_type(i, parts[i].x, parts[i].y, PT_PLNT);
					break;
				case PT_RBDM:
				case PT_LRBD:
					if ((sim->legacy_enable || parts[i].temp>(273.15f+12.0f)) && RNG::Ref().chance(1, 100)) {
						sim->part_change_type(i,x,y,PT_FIRE);
						parts[i].life = 4;
						parts[i].ctype = PT_WATR;
					}
					break;
				case PT_FIRE:
					if (parts[ID(r)].ctype != PT_WATR) {
						sim->kill_part(ID(r));
						if (RNG::Ref().chance(1, 30)) {
							sim->kill_part(i);
							return 1;
						}
					}
					break;
				case PT_YEST:
					if (RNG::Ref().chance(1, 300)) {
						sim->part_change_type(i, parts[i].x, parts[i].y, PT_YEST);
						return 0;
					}
				case PT_NONE:
					break;
				default:
					continue;
				}
			}
	return 0;
}
