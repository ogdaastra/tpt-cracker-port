#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_SICN() {
	Identifier = "DEFAULT_PT_SICN";
	Name = "SLCN2"; // "SLCN" in TPT Ultimata; renamed because the official SLCN exists
	Colour = PIXPACK(0x628099);
	MenuVisible = 1;
	MenuSection = SC_ELEC;
	Enabled = 1;

	Advection = 0.00f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 1.00f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 1;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 251;
	Description = "Silicon. Temporarily turns on powered elements when sparked, cannot conduct spark on diagonals.";

	Properties = TYPE_SOLID | PROP_CONDUCTS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15f + 1414.0f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, r, rt;
	for (rx = -1; rx <= 1; ++rx)
	for (ry = -1; ry <= 1; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (!r) continue;
			rt = TYP(r);

			// Doping
			// N-type:
			if (rt == PT_BSMH || (rt == PT_LAVA && parts[ID(r)].ctype == PT_BSMH) ||
				rt == PT_PHSP || (rt == PT_LAVA && parts[ID(r)].ctype == PT_PHSP)) {
				if (parts[i].temp > 273.15f + 400.0f && RNG::Ref().chance(1, 1000)) {
					sim->part_change_type(i, x, y, PT_NSCN);
					return 1;
				}
			}

			// Ptype
			else if (rt == PT_ALMN || (rt == PT_LAVA && parts[ID(r)].ctype == PT_ALMN) || rt == PT_ION) {
				if (parts[i].temp > 273.15f + 400.0f && RNG::Ref().chance(1, 1000)) {
					sim->part_change_type(i, x, y, PT_PSCN);
					return 1;
				}
			}

			// Deactivate powered materials (SPRK(SICN) activates powered)
			bool is_powered = rt == PT_SWCH || sim->elements[rt].MenuSection == SC_POWERED;
			if (is_powered && parts[i].life > 0 && parts[ID(r)].life > 0) {
				PropertyValue value;
				value.Integer = 0;
				sim->flood_prop(x + rx, y + ry, offsetof(Particle, life), value, StructProperty::Integer);

				// LCRY needs tmp & tmp2 for some reason
				if (TYP(r) == PT_LCRY) {
					value.Integer = 0;
					sim->flood_prop(x + rx, y + ry, offsetof(Particle, tmp), value, StructProperty::Integer);
					sim->flood_prop(x + rx, y + ry, offsetof(Particle, tmp2), value, StructProperty::Integer);
				}
			}
		}

	return 0;
}
