#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_RCK() {
	Identifier = "DEFAULT_PT_RCK";
	Name = "ROCK2"; // "ROCK" in TPT Ultimata; renamed because the official ROCK exists (cf. LITH2)
	Colour = PIXPACK(0x938F80);
	MenuVisible = 1;
	MenuSection = SC_SOLIDS;
	Enabled = 1;

	Advection = 0.2f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.90f;
	Collision = 0.0f;
	Gravity = 0.5f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 121;
	Description = "Bowser's Rock. Tougher than brick, erodes slowly from moving water. Can be melted and refined into metals.";

	Properties = TYPE_PART | PROP_HOT_GLOW;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Create = &create;
	Update = &update;
	Graphics = &graphics;
}

static void create(ELEMENT_CREATE_FUNC_ARGS) {
	// Ores:
	// COAL, IRON, TTAN, GOLD, RDMD, URAN, PLUT, TUNG, STNE
	int ore = RNG::Ref().between(0, 100);
	if (ore < 2) // Diamond ore
		sim->parts[i].ctype = PT_RDMD;
	else if (ore < 4)
		sim->parts[i].ctype = PT_PLUT;
	else if (ore < 6)
		sim->parts[i].ctype = PT_GOLD;
	else if (ore < 8)
		sim->parts[i].ctype = PT_URAN;
	else if (ore < 30)
		sim->parts[i].ctype = PT_IRON;
	else if (ore < 35)
		sim->parts[i].ctype = PT_BMTL;
	else if (ore < 40)
		sim->parts[i].ctype = PT_COAL;
	else if (ore < 42)
		sim->parts[i].ctype = PT_TUNG;
	else
		sim->parts[i].ctype = PT_STNE;

	sim->parts[i].tmp = RNG::Ref().between(0, 6);
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties
	 * - tmp   - Used for graphics
	 * - ctype - Type of ORE
	 * - tmp2  - Is powder form
	 */
	bool valid_ctype = parts[i].ctype > 0 && parts[i].ctype < PT_NUM && sim->elements[parts[i].ctype].Enabled;

	// High pressure, fragment and release gas
	if (sim->pv[y / CELL][x / CELL] > 12.0f) {
		int change_to = 0;
		if (parts[i].ctype == PT_COAL)
			change_to = PT_BCOL;
		else if (parts[i].ctype == PT_RDMD)
			change_to = PT_PQRT;
		else if (parts[i].ctype == PT_TUNG)
			change_to = PT_BRMT;
		else if (RNG::Ref().chance(1, 500))
			change_to = PT_OIL;
		else if (RNG::Ref().chance(1, 500))
			change_to = PT_GAS;
		else if (valid_ctype &&
				(sim->elements[parts[i].ctype].Properties & TYPE_PART ||
				 sim->elements[parts[i].ctype].Properties & TYPE_LIQUID ||
				 sim->elements[parts[i].ctype].Properties & TYPE_GAS))
			change_to = parts[i].ctype;
		
		if (change_to) {
			sim->part_change_type(i, x, y, change_to);
			return 1;
		} else {
			parts[i].tmp2 = 1;
		}
	}

	// Melt into the ore
	// If ore can't melt melt into regular lava
	if (valid_ctype && sim->elements[parts[i].ctype].Meltable && parts[i].temp > sim->elements[parts[i].ctype].HighTemperature) {
		sim->part_change_type(i, x, y, PT_LAVA);
		return 1;
	}
	else if (valid_ctype && !sim->elements[parts[i].ctype].Meltable && parts[i].temp > 1273.15f) {
		parts[i].ctype = PT_ROCK;
		sim->part_change_type(i, x, y, PT_LAVA);
		return 1;
	}

	int rx, ry, r, rt;
	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				// Expanding ICE may crack stone
				if (rt == PT_ICEI && RNG::Ref().chance(1, 1000))
					parts[i].tmp2 = 1;

				// Convert WATR into SLTW
				else if ((rt == PT_WATR || rt == PT_DSTW) && RNG::Ref().chance(1, 100))
					sim->part_change_type(ID(r), x + rx, y + ry, PT_SLTW);

				// Erode
				if (sim->elements[rt].Properties & PROP_WATER
						&& RNG::Ref().chance(1, 2000) &&
						(fabs(parts[ID(r)].vy) + fabs(parts[ID(r)].vx) > 0.1f)) {
					sim->kill_part(i);
					return 1;
				}
				
				// Chance to rust randomly from O2
				else if ((rt == PT_O2) && RNG::Ref().chance(1, 5000)) {
					sim->part_change_type(i, x, y, PT_BMTL);
					return 1;
				}
			}

	if (!parts[i].tmp2) {
		parts[i].vx = parts[i].vy = 0.0f;
		return 1;
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	int m = (cpart->tmp - 3) * 10;
	*colr += m;
	*colg += m;
	*colb += m;

	return 0;
}