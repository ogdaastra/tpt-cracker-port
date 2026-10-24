#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
void Element_CLST_create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_CLCM() {
	Identifier = "DEFAULT_PT_CLCM";
	Name = "CLCM";
	Colour = PIXPACK(0xE7E7E7);
	MenuVisible = 1;
	MenuSection = SC_POWDERS;
	Enabled = 1;

	Advection = 0.4f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 5;
	Explosive = 0;
	Meltable = 5;
	Hardness = 1;

	Weight = 90;

	HeatConduct = 150;
	Description = "Calcium. Ignites when finely divided, makes hydrogen with water.";

	Properties = TYPE_PART | PROP_HOT_GLOW;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 842.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
	Graphics = &graphics;
	Create = &Element_CLST_create;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, r, rt;
	int other_ca_count = 0;

	for (rx = -1; rx < 2; ++rx)
	for (ry = -1; ry < 2; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (!r) continue;
			rt = TYP(r);

			if (rt == PT_WATR || rt == PT_DSTW || rt == PT_SWTR || rt == PT_SLTW || rt == PT_CBNW || rt == PT_IOSL || rt == PT_WTRV) {
				sim->part_change_type(i, x, y, PT_H2);
				return 1;
			}
			else if (rt == PT_CLCM) {
				other_ca_count++;
			}
		}

	if (other_ca_count == 0 && RNG::Ref().chance(1, 500)) {
		sim->part_change_type(i, x, y, PT_FIRE);
		parts[i].temp += 500.0f;
		parts[i].life = RNG::Ref().between(50, 200);
		return 1;
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	int z = (cpart->tmp - 5) * 8; // Speckles!
	*colr += z;
	*colg += z;
	*colb += z;

	return 0;
}
