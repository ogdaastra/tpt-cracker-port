#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

#define CLOT 180

void Element::Element_BLOD() {
	Identifier = "DEFAULT_PT_BLOD";
	Name = "BLOD";
	Colour = PIXPACK(0xEB1515);
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
	HotAir = 0.000f * CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 35;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
	DefaultProperties.life = 100;
	DefaultProperties.tmp = 2;

	HeatConduct = 29;
	Description = "Blood. Stains particles, clots when still.";

	Properties = TYPE_LIQUID | PROP_NEUTPENETRATE;

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
	/**
	 * Properties:
	 * life - How much oxygen stored
	 * tmp - How many particles it can still stain
	 * tmp2 - Visocity, slowly increases randomly with contact to air
	 */

	// Boundng
	if (parts[i].life < 0)
		parts[i].life = 0;
	if (parts[i].life > 100)
		parts[i].life = 100;

	// Freezing
	if (parts[i].temp < 273.15f) {
		sim->part_change_type(i, parts[i].x, parts[i].y, PT_ICEI);
		parts[i].ctype = PT_BLOD;
		parts[i].dcolour = sim->elements[PT_BLOD].Colour + 0x77000000;
		return 0;
	}

	// Boiling
	if (parts[i].temp > 273.15f + 100.0f) {
		if (RNG::Ref().chance(1, 500))
			sim->part_change_type(i, parts[i].x, parts[i].y, PT_BRMT);
		else {
			sim->part_change_type(i, parts[i].x, parts[i].y, PT_WTRV);
			parts[i].dcolour = sim->elements[PT_BLOD].Colour + 0x44000000;
		}
	}

	// Clotted blood is inert
	if (parts[i].tmp2 >= CLOT) {
		parts[i].life--;
		if (parts[i].life < 30)
			parts[i].life = 30;
		parts[i].vx = parts[i].vy = 0;
	}

	int rx, ry, r, rt;
	unsigned int newcolor;
	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];

				if (TYP(r) == PT_SOAP && RNG::Ref().chance(1, 100)) {
					sim->kill_part(i);
					continue;
				}

				// Clotted blood has none of these updates
				if (parts[i].tmp2 >= CLOT)
					continue;

				if (!r) {
					// Random visocity increase if not moving fast and minimal pressure
					if (fabs(parts[i].vx) < 0.1f && fabs(parts[i].vy) < 0.1f &&
							fabs(sim->pv[y / CELL][x / CELL]) < 1.0f)
						parts[i].tmp2 += RNG::Ref().between(1, 7);
					continue;
				}
				rt = TYP(r);

				// Oxygenate
				if (rt == PT_O2 && parts[i].life < 100) {
					parts[i].life += 10;
					sim->kill_part(ID(r));
				}
				// Spread oxygen to surrounding BLOD particles
				else if (rt == PT_BLOD && parts[i].life - 2 > parts[ID(r)].life && parts[ID(r)].tmp2 < CLOT) {
					parts[i].life = (parts[ID(r)].life + parts[i].life + 1) / 2;
					parts[ID(r)].life = parts[i].life;
				}
				// Stain powders and solids
				else if (rt != PT_ICEI && rt != PT_SNOW && rt != PT_BIZRS &&
						(sim->elements[rt].Properties & TYPE_PART || sim->elements[rt].Properties & TYPE_SOLID)) {
					newcolor = 0xFF000000 + sim->elements[PT_BLOD].Colour;
					if (parts[ID(r)].dcolour != newcolor && parts[i].tmp > 0) {
						--parts[i].tmp;
						parts[ID(r)].dcolour = newcolor;
					}
				}
				// Stain liquids
				// Liquids get stained more, but in a diluted color
				else if (rt != PT_BLOD && rt != PT_SOAP && rt != PT_BIZR && sim->elements[rt].Properties & TYPE_LIQUID) {
					if (parts[i].tmp > 0) {
						newcolor = 0xAA000000 + sim->elements[PT_BLOD].Colour;
						if (parts[ID(r)].dcolour != newcolor && RNG::Ref().chance(1, 2)) {
							--parts[i].tmp;
							parts[ID(r)].dcolour = newcolor;
						}
					}
				}
				// Chance to kill VIRS
				else if (RNG::Ref().chance(1, 10) && (rt == PT_VIRS || rt == PT_VRSG || rt == PT_VRSS))
					sim->kill_part(ID(r));
				// Chance to kill BCTR
				else if (RNG::Ref().chance(1, 10) && rt == PT_BCTR)
					sim->kill_part(ID(r));
			}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->tmp2 < CLOT)
		*pixel_mode |= PMODE_BLUR;
	*colr *= 0.2f + 0.8f * cpart->life / 100.0f;
	*colg *= 0.2f + 0.8f * cpart->life / 100.0f;
	*colb *= 0.2f + 0.8f * cpart->life / 100.0f;

	return 0;
}
