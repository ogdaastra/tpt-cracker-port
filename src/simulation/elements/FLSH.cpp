#include "simulation/ElementCommon.h"

int Element_FLSH_update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_FLSH() {
	Identifier = "DEFAULT_PT_FLSH";
	Name = "FLSH";
	Colour = PIXPACK(0xF05B5B);
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

	Flammable = 3;
	Explosive = 0;
	Meltable = 0;
	Hardness = 15;

	Weight = 100;

	DefaultProperties.tmp = 1000;
	DefaultProperties.tmp2 = 1000;

	HeatConduct = 104;
	Description = "Flesh. Absorbs food and oxygen from BLOD and STMH, can be cooked.";

	Properties = TYPE_SOLID | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 600.0f + 273.15f;
	HighTemperatureTransition = PT_FIRE;

	Update = &Element_FLSH_update;
	Graphics = &graphics;
	Create = &create;
}

static void create(ELEMENT_CREATE_FUNC_ARGS) {
	int gx = x / 50 * 50;
	int gy = y / 50 * 50;
	float theta = x + y;

	int newx = cos(theta) * (x - gx) - sin(theta) * (y - gy) + x;
	int newy = sin(theta) * (x - gx) + cos(theta) * (y - gy) + y;
	x = newx, y = newy;

	int thickness = (x / 4 + y / 4) % 5;
	if ((x + y) % 25 < thickness || abs(x - y) % 25 < thickness) {
		sim->parts[i].life = 1; // White part
	}
}

int Element_FLSH_update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties
	 * life:  Graphics
	 * tmp:   Oxygen stored
	 * tmp2:  Nutrients stored
	 * tmp3: Highest temperature
	 * tmp4: Type 0 = inside, 1 = skin, 2 = dead
	 */

	if (parts[i].tmp4 != 2) {
		if (parts[i].tmp > 0)
			parts[i].tmp--;
		if (parts[i].tmp2 > 0)
			parts[i].tmp2--;
	}

	// The randomization is to avoid easily burning meat and creating
	// a more realistic crystallization effect
	if (parts[i].temp > 110.0f + 273.15f && RNG::Ref().chance(1, 100))
		parts[i].tmp3 = (int)parts[i].temp;
	if (parts[i].temp > parts[i].tmp3 && (parts[i].temp < 110.0f + 273.15f ||
			parts[i].temp > 150.0f + 273.15f))
		parts[i].tmp3 = (int)parts[i].temp;
	
	// Rot if dead and not cooked or frozen
	if (parts[i].temp > 3.0f + 273.15f && parts[i].tmp3 < 40 + 273 && parts[i].tmp4 == 2
			&& RNG::Ref().chance(1, 5000)) {
		sim->part_change_type(i, x, y, PT_BCTR);
		parts[i].ctype = 512; // Meat eating gene
		parts[i].tmp = 0;
		parts[i].tmp2 = 0;
		return 1;
	}

	// Death
	// Radiation burns
	int r = sim->photons[y][x];
	if (r && TYP(r) != PT_JCB1 && TYP(r) != PT_NTRI && RNG::Ref().chance(1, 10)) {
		parts[i].tmp4 = 2;
		if (parts[ID(r)].temp > 273.15f + 110.0f)
			parts[i].tmp3 = 273 + 150;
	}
	// Pressure
	if (fabs(sim->pv[y / CELL][x / CELL]) > 10.0f)
		parts[i].tmp4 = 2;
	// Temperature
	if (parts[i].temp < 273.15f - 5.0f || parts[i].temp > 50.0f + 273.15f)
		parts[i].tmp4 = 2;
	// Lack of oxygen or nutrients
	if ((parts[i].tmp <= 0 || parts[i].tmp2 <= 0) && RNG::Ref().chance(1, 3000))
		parts[i].tmp4 = 2;

	int rx, ry, rt;

	for (rx = -1; rx < 2; ++rx)
	for (ry = -1; ry < 2; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (!r) {
				// Alive flesh
				if (parts[i].tmp4 != 2 && RNG::Ref().chance(1, 1000)) {
					// Create skin
					parts[i].tmp4 = 1;
				}
				continue;
			}
			rt = TYP(r);

			// Alive flesh
			if (parts[i].tmp4 != 2) {
				// Distribute nutrients
				if (rt == PT_FLSH || rt == PT_UDDR || rt == PT_STMH) {
					int diff = parts[i].tmp - parts[ID(r)].tmp;
					parts[i].tmp -= diff / 2;
					parts[ID(r)].tmp += (diff + 1) / 2;
					diff = parts[i].tmp2 - parts[ID(r)].tmp2;
					parts[i].tmp2 -= diff / 2;
					parts[ID(r)].tmp2 += (diff + 1) / 2;
				}

				// Die if touching toxic chemicals
				else if (rt == PT_H2O2 || rt == PT_ACID || rt == PT_CAUS || rt == PT_PLUT || rt == PT_URAN ||
					rt == PT_ISOZ || rt == PT_ISZS || rt == PT_POLO || rt == PT_MERC) {
					parts[i].tmp4 = 2;
				}

				// Absorb oxygen from blood
				// Pretend there are some nutrients in the blood
				if (rt == PT_BLOD) {
					int amount = std::min(50, parts[ID(r)].life);
					parts[i].tmp += amount;
					parts[i].tmp2 += 10;
					parts[ID(r)].life -= amount;
				}
			}
		}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->tmp4 == 1) { // Skin
		*colr = 255;
		*colg = 226;
		*colb = 204;
	}
	else if (cpart->life == 1) { // White part
		*colr = 255;
		*colg = 238;
		*colb = 230;
	}
	else { // Redden if oxygenated
		int red = std::min(40, cpart->tmp / 10);
		*colr += red;
		*colg -= red;
		*colb -= red;
	}

	// Cooking
	// Well done (Around 70 - 80 C)
	if (cpart->tmp3 > 273 + 40) {
		float percent_fade = std::min(cpart->tmp3 - 273.15f, 80.0f) / 80.0f;
		percent_fade += ((abs(nx - ny) * (nx + ny) + nx) % 5) / 10.0f; // Noise

		*colr -= (*colr - 160) * percent_fade;
		*colg -= (*colg - 96) * percent_fade;
		*colb -= (*colb - 69) * percent_fade;

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
