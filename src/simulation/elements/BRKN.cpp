#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_BRKN() {
	Identifier = "DEFAULT_PT_BRKN";
	Name = "BRKN";
	Colour = PIXPACK(0x705060);
	MenuVisible = 0;
	MenuSection = SC_SPECIAL;
	Enabled = 1;

	Advection = 0.2f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 2;

	Weight = 90;

	HeatConduct = 211;
	Description = "A generic broken solid.";

	Properties = TYPE_PART | PROP_CONDUCTS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	parts[i].tmp = parts[i].ctype;

	bool flammable = false;
	bool is_aluminium = parts[i].ctype == PT_ALMN;

	if (parts[i].ctype > 0 && parts[i].ctype < PT_NUM) {
		if (!(sim->elements[parts[i].ctype].Properties & PROP_CONDUCTS))
			parts[i].life = 4; // Prevent spark conducting if not actually conductable
		if (sim->elements[parts[i].ctype].Update)
			sim->elements[parts[i].ctype].Update(sim, i, x, y, surround_space, nt, parts, pmap);
		if (sim->elements[parts[i].ctype].HighTemperatureTransition &&
				parts[i].temp > sim->elements[parts[i].ctype].HighTemperature) {
			sim->part_change_type(i, x, y, sim->elements[parts[i].ctype].HighTemperatureTransition);
			return 1;
		}
		flammable = sim->elements[parts[i].ctype].Flammable > 0;
	}

	for (int rx = -1; rx <= 1; ++rx)
	for (int ry = -1; ry <= 1; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			int r = pmap[y + ry][x + rx];
			if (!r) continue;
			int rt = TYP(r);
			bool is_water = rt == PT_IOSL || rt == PT_WATR || rt == PT_DSTW || rt == PT_SLTW || rt == PT_CBNW || rt == PT_SWTR || rt == PT_WTRV;

			if (flammable && (rt == PT_FIRE || rt == PT_PLSM)) {
				sim->part_change_type(i, x, y, PT_FIRE);
				parts[i].temp += 200.0f;
				return 1;
			}
			else if (parts[i].ctype == PT_SAWD && is_water) {
				sim->part_change_type(i, x, y, PT_PULP);
				parts[i].life += 1000;
				return 1;
			}
			else if (is_aluminium && rt == PT_BRMT && parts[i].temp > 240.0f + 273.15f && RNG::Ref().chance(1, 100)) {
				sim->part_change_type(ID(r), x + rx, y + ry, PT_THRM);
				sim->part_change_type(i, x, y, PT_THRM);
				return 1;
			}
		}

	if (parts[i].ctype == PT_PAPR)
		parts[i].dcolour = 0;

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->ctype > 0 && cpart->ctype < PT_NUM) {
		*colr = PIXR(ren->sim->elements[cpart->ctype].Colour);
		*colg = PIXG(ren->sim->elements[cpart->ctype].Colour);
		*colb = PIXB(ren->sim->elements[cpart->ctype].Colour);

		if (ren->sim->elements[cpart->ctype].Graphics)
			ren->sim->elements[cpart->ctype].Graphics(ren, cpart, nx, ny, pixel_mode, cola,
				colr, colg, colb, firea, firer, fireg, fireb);
	}
	return 0;
}
