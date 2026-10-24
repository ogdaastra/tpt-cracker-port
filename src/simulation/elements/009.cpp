#include "simulation/ElementCommon.h"

int Element_009_update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_009() {
	Identifier = "DEFAULT_PT_009";
	Name = "009";
	Colour = PIXPACK(0xF04848);
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

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 30;

	DefaultProperties.temp = -R_TEMP + 2.0f + 273.15f;
	HeatConduct = 29;
	Description = "SCP-009. Water with reversed enthalpy changes.";

	Properties = TYPE_LIQUID|PROP_CONDUCTS|PROP_LIFE_DEC|PROP_NEUTPASS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = 173.15f;
	LowTemperatureTransition = PT_009G;
	HighTemperature = 273.0f;
	HighTemperatureTransition = PT_009S;

	Update = &Element_009_update;
	Graphics = &graphics;
}

int Element_009_update(UPDATE_FUNC_ARGS) {
	int rx, ry, r, rt;

	for (rx = -1; rx < 2; ++rx)
	for (ry = -1; ry < 2; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (!r) continue;
			rt = TYP(r);

			if (sim->elements[rt].Properties & PROP_WATER ||
				rt == PT_MILK || rt == PT_PULP || rt == PT_GLUE || rt == PT_WTRV || rt == PT_MUD || rt == PT_BCTR ||
				rt == PT_BLOD || rt == PT_HONY || rt == PT_VNGR) {
					sim->part_change_type(ID(r), x + rx, y + ry, PT_009);
					parts[ID(r)].tmp = 0;
					parts[ID(r)].tmp2 = 0;
					continue;
				}
			else if (rt == PT_PLNT || rt == PT_BEE || rt == PT_FISH || rt == PT_SPDR || rt == PT_ANT || rt == PT_BIRD ||
					(rt == PT_ICEI || rt == PT_SNOW) || rt == PT_FLSH || rt == PT_POTO || rt == PT_ALGE ||
					 rt == PT_UDDR || rt == PT_STMH) {
					if (RNG::Ref().chance(1, 200)) {
						sim->part_change_type(ID(r), x + rx, y + ry, PT_009);
						parts[ID(r)].tmp = 0;
						parts[ID(r)].tmp2 = 0;
						continue;
					}
				}
		}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*pixel_mode |= PMODE_BLUR;
	return 1;
}
