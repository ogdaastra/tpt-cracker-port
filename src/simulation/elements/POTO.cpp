#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
void Element_CLST_create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_POTO() {
	Identifier = "DEFAULT_PT_POTO";
	Name = "POTO";
	Colour = PIXPACK(0xB08464);
	MenuVisible = 1;
	MenuSection = SC_POWDERS; // SC_ORGANIC in TPT Ultimata
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
	Meltable = 0;
	Hardness = 20;

	Weight = 50;

	HeatConduct = 150;
	Description = "Potatoes.";

	DefaultProperties.life = 4;

	Properties = TYPE_PART | PROP_EDIBLE;
	FoodValue = 10;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15 + 600.0f;
	HighTemperatureTransition = PT_FIRE;

	Update = &update;
	Graphics = &graphics;
	Create = &Element_CLST_create;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * tmp3 - max temp
	 * life  - water content
	 * tmp   - graphics
	 */
	if (parts[i].temp > parts[i].tmp3)
		parts[i].tmp3 = (int)parts[i].temp;

	if (parts[i].temp > 100.0f + 273.15f && parts[i].life > 0) {
		for (int rx = -1; rx <= 1; ++rx)
		for (int ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				int r = pmap[y + ry][x + rx];
				if (!r) {
					parts[i].life--;
					sim->create_part(-1, x + rx, y + ry, PT_WTRV);
					return 0;
				}
			}
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
