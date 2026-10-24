#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
int Element_FLSH_update(UPDATE_FUNC_ARGS);

void Element::Element_STMH() {
	Identifier = "DEFAULT_PT_STMH";
	Name = "STMH";
	Colour = PIXPACK(0xc4AE95);
	MenuVisible = 1;
	MenuSection = SC_SOLIDS; // SC_ORGANIC in TPT Ultimata
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 10;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 100;

	HeatConduct = 104;
	Description = "Stomach wall. Digests food and sends nutrients to flesh.";

	Properties = TYPE_SOLID | PROP_NEUTPENETRATE;

	DefaultProperties.tmp = 1000;
	DefaultProperties.tmp2 = 1000;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 200.0f + 273.15f;
	HighTemperatureTransition = PT_FIRE;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties
	 * life:  Graphics
	 * tmp:   Oxygen stored
	 * tmp2:  Nutrients stored
	 * tmp3: Highest temperature
	 * tmp4: Type 0 = inside, 1 = skin, 2 = dead
	 */
	Element_FLSH_update(sim, i, x, y, surround_space, nt, parts, pmap);
	if (parts[i].tmp4 == 1) // Override skin formation
		parts[i].tmp4 = 0;

	if (parts[i].tmp4 != 2) {
		int rx, ry, r, rt;
		for (rx = -1; rx < 2; ++rx)
		for (ry = -1; ry < 2; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);
				if (rt == PT_BRKN)
					rt = parts[ID(r)].ctype;

				// Consume food
				if (rt == PT_PLNT || rt == PT_VINE || rt == PT_SEED || rt == PT_POTO || rt == PT_HONY ||
					rt == PT_SPDR || rt == PT_ANT || rt == PT_BEE || rt == PT_SUGR) {
					if (RNG::Ref().chance(1, 500)) {
						parts[i].tmp += 50;
						parts[i].tmp2 += 500;
						sim->kill_part(ID(r));
					}
				}
			}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Redden if oxygenated
	int red = std::min(20, cpart->tmp / 10);
	*colr += red;
	*colg -= red;
	*colb -= red;

	// Cooking
	// Well done (Around 70 - 80 C)
	if (cpart->tmp3 > 273 + 40) {
		float percent_fade = std::min(cpart->tmp3 - 273.15f, 80.0f) / 80.0f;
		percent_fade += ((abs(nx - ny) * (nx + ny) + nx) % 5) / 10.0f; // Noise

		*colr -= (*colr - 176) * percent_fade;
		*colg -= (*colg - 131) * percent_fade;
		*colb -= (*colb - 90) * percent_fade;

		// Grill lines
		if ((nx + ny) % 30 < 3) {
			*colr *= 0.9f, *colg *= 0.9f, *colb *= 0.9f;
		}
	}
	// Burnt (Above 110 C)
	if (cpart->tmp3 > 273 + 110) {
		float m = 1.0f - std::min(cpart->tmp3 - 273.15f + 90.0f, 200.0f) / 200.0f;
		m = 0.2 + 0.8 * m; // Prevent 100% black
		*colr *= m, *colg *= m, *colb *= m;
	}
	// Blue when cold
	if (cpart->temp < 273 && cpart->tmp3 < 273 + 110) {
		*colr -= (int)restrict_flt((273-cpart->temp)/5,0,80);
		*colg += (int)restrict_flt((273-cpart->temp)/4,0,40);
		*colb += (int)restrict_flt((273-cpart->temp)/1.5,0,100);
	}

	return 0;
}
