#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_HONY() {
	Identifier = "DEFAULT_PT_HONY";
	Name = "HONY";
	Colour = PIXPACK(0xFF9900);
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
	HotAir = 0.000f  * CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 35;

	DefaultProperties.temp = 30.0f + 273.15f;
	HeatConduct = 29;
	Description = "Honey. Sweet liquid produced by BEEs.";

	Properties = TYPE_LIQUID | PROP_EDIBLE;
	FoodValue = 5;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 71.f + 273.15f;
	HighTemperatureTransition = PT_SWTR;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, r, rt;
	float stickx = 0.0f, sticky = 0.0f;

	for (rx = -1; rx < 2; ++rx)
		for (ry = -1; ry < 2; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				// Dont move if near wax to avoid leaking from honeycombs
				if (rt == PT_WAX)
					parts[i].vx = parts[i].vy = 0;
				
				// Slowly kill BCTR
				else if (rt == PT_BCTR && RNG::Ref().chance(1, 1000))
					sim->kill_part(ID(r));

				// "Stick" to solids and itself
				else if (rt == PT_HONY || sim->elements[rt].Properties & TYPE_SOLID) {
					stickx += rx;
					sticky += ry;
				}
			}

	// Move torwards stick direction
	parts[i].vx += stickx / 50.0f;
	parts[i].vy += sticky / 5.0f;

	// Freeze if cold
	if (parts[i].temp < 273.15f - 20.0f) {
		parts[i].vx = parts[i].vy = 0;
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->temp> 273.15f - 20.0f)
		*pixel_mode |= PMODE_BLUR; // Looks like liquid if not cold
	return 0;
}
