#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
void Element_CLST_create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_CMTP() {
	Identifier = "DEFAULT_PT_CMTP";
	Name = "CMTP";
	Colour = PIXPACK(0x707070);
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

	Flammable = 0;
	Explosive = 0;
	Meltable = 5;
	Hardness = 1;
	Weight = 90;

	HeatConduct = 150;
	Description = "Cement Powder. Mix with water to make liquid concrete.";

	Properties = TYPE_PART | PROP_DEADLY;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 983.0f;
	HighTemperatureTransition = PT_LAVA;

	Create = &Element_CLST_create;
	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, r, rt;
	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				// Touching water
				if (sim->elements[rt].Properties & PROP_WATER) {
					sim->part_change_type(i, x, y, PT_CMNT);

					// Sugar water and salt water are impure and will make low quality concrete
 					if (rt == PT_SWTR || rt == PT_SLTW || rt == PT_IOSL || rt == PT_CBNW)
						parts[i].tmp2 = 100;
					parts[i].life += 500;
					return 1;
				}
			}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	int z = (cpart->tmp - 5) * 4; //speckles!
	*colr += z;
	*colg += z;
	*colb += z;

	return 0;
}
