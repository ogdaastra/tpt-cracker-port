#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
void Element_PROT_create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_APRT() {
	Identifier = "DEFAULT_PT_APRT";
	Name = "APRT";
	Colour = PIXPACK(0x7A14FF);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 1.00f;
	Loss = 1.00f;
	Collision = -.99f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = -1;

	HeatConduct = 61;
	Description = "Anti-Protons. Annihilates on contact with regular matter, sparks conductors.";

	Properties = TYPE_ENERGY;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	DefaultProperties.life = 75;

	Update = &update;
	Graphics = &graphics;
	Create = &Element_PROT_create;
}

static int update(UPDATE_FUNC_ARGS) {
	int r, rt, rx, ry, ni;
	float total_e, vx, vy, angle, difference, direction1, direction2;

	sim->pv[y/CELL][x/CELL] += .003f;

	for (rx=-2; rx<=2; rx++)
	for (ry=-2; ry<=2; ry++)
		if (BOUNDS_CHECK) {
			r = pmap[y+ry][x+rx];
			if (!r)
				r = sim->photons[y + ry][x + rx];
			if (!r) continue;
			rt = TYP(r);

			// Under check
			if (rx == 0 && ry == 0) {
				switch(rt) {
					case PT_LCRY:
						//Powered LCRY reaction: APRT->PHOT
						if (parts[ID(r)].life > 5 && RNG::Ref().chance(1, 10)) {
							sim->part_change_type(i, x, y, PT_PHOT);
							parts[i].life *= 2;
							parts[i].ctype = 0x3FFFFFFF;
							return 1;
						}
						break;
					case PT_INVIS:
						if (RNG::Ref().chance(1, 10)) {
							sim->part_change_type(i, x, y, PT_NEUT);
							return 1;
						}
				}
			}

			switch(rt) {
				case PT_ELEC: // Repel
					parts[ID(r)].vx += rx;
					parts[ID(r)].vy += ry;
					parts[i].vx -= rx;
					parts[i].vy -= ry;
					break;
				case PT_PROT: // Turn into 2 PHOT based on kinetic energy
					total_e = sqrtf(parts[i].vx*parts[i].vx+parts[i].vy*parts[i].vy)
						+ sqrtf(parts[ID(r)].vx*parts[ID(r)].vx+parts[ID(r)].vy*parts[ID(r)].vy);
					parts[ID(r)].temp += total_e / 2 * 700.0f;
					parts[i].temp += total_e / 2 * 700.0f;

					angle = RNG::Ref().between(0, 359) / 180.0f * 3.1415f;
					vx = total_e / 2 * cos(angle);
					vy = total_e / 2 * sin(angle);

					parts[ID(r)].vx = vx;
					parts[ID(r)].vy = vy;
					parts[i].vx = -vx;
					parts[i].vy = -vy;

					parts[ID(r)].life = parts[i].life = 1000;
					parts[ID(r)].ctype = parts[i].ctype = 0x3F000000 >> std::min(29, (int)total_e);

					sim->part_change_type(i, x, y, PT_PHOT);
					sim->part_change_type(ID(r), x + rx, y + ry, PT_PHOT);
					
					ni = sim->create_part(-3, x + rx / 2, y + ry / 2, PT_EMBR);
					if (ni > -1) {
						parts[ni].life = 70;
						parts[ni].tmp = 3;
						parts[ni].temp = MAX_TEMP;
					}
					return 1;
					break;
				case PT_PLUT:
					if (RNG::Ref().chance(1, 20)) {
						sim->part_change_type(ID(r), x + rx, y + ry, PT_URAN);
						goto annihilate;
					}
					break;
				case PT_URAN:
					if (RNG::Ref().chance(1, 20)) {
						sim->part_change_type(ID(r), x + rx, y + ry, PT_POLO);
						goto annihilate;
					}
					break;
				case PT_POLO:
					if (RNG::Ref().chance(1, 20)) {
						sim->part_change_type(ID(r), x + rx, y + ry, PT_STNE);
						goto annihilate;
					}
					break;
				// Particle collisions
				case PT_APRT:
					// Should be mostly opposite directions
					direction1 = atan2f(-parts[i].vy, parts[i].vx);
					direction2 = atan2f(-parts[ID(r)].vy, parts[ID(r)].vx);
					difference = direction1 - direction2; if (difference < 0) difference += 6.28319f;

					if (difference > 3.12659f && difference < 3.15659f) {
						total_e = sqrtf(parts[i].vx*parts[i].vx+parts[i].vy*parts[i].vy)
							+ sqrtf(parts[ID(r)].vx*parts[ID(r)].vx+parts[ID(r)].vy*parts[ID(r)].vy);

						if (total_e > 70)
							sim->part_change_type(i, x, y, PT_SING);
						else if (total_e > 20) {
							sim->part_change_type(i, x, y, PT_AMTR);
							parts[i].tmp2 = 1;
						}
						else if (total_e >= 10)
							sim->part_change_type(i, x, y, PT_ANH2);
						else // Not fast enough
							break;
						parts[i].temp += total_e * 700.0f;
						sim->kill_part(ID(r));
						return 1;
					}
					break;
				case PT_NONE:
					break;
				default:
					if (sim->elements[rt].Properties & PROP_WATER) {
						if (RNG::Ref().chance(1, 3))
							sim->create_part(ID(r), x+rx, y+ry, PT_O2);
						else
							sim->create_part(ID(r), x+rx, y+ry, PT_H2);
						goto annihilate;
					}

					if ((sim->elements[rt].Properties & PROP_CONDUCTS) && rt != PT_MMSH && (rt!=PT_NBLE || parts[i].temp < 2273.15)) {
						sim->create_part(-1, x+rx, y+ry, PT_SPRK);
						goto annihilate;
					}

					if (!(sim->elements[rt].Properties & PROP_INDESTRUCTIBLE) && rt != PT_CLNE && rt != PT_BCLN
						&& rt != PT_EMBR && rt != PT_CLNE && rt != PT_PCLN && rt != PT_BCLN && !(sim->elements[rt].Properties & TYPE_ENERGY)
						&& rt != PT_INVIS && rt != PT_LCRY && rt != PT_ANH2 && rt != PT_AMTR && rt != PT_BHOL && rt != PT_WHOL
						&& rt != PT_VOID && rt != PT_PVOD && rt != PT_WARP && rt != PT_SING && rt != PT_NBHL && rt != PT_NWHL)
						goto annihilate;
					break;
			}
		}

	// Annihilate with normal matter
	if (false) {
		annihilate:
		sim->kill_part(i);
		ni = sim->create_part(-3, x, y, PT_EMBR);
		if (ni > -1) {
			parts[ni].life = 70;
			parts[ni].tmp = 3;
			parts[ni].temp = MAX_TEMP;
		}
		return 1;
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*firea = 15;
	*firer = 58;
	*fireg = 150;
	*fireb = 220;

	*pixel_mode |= FIRE_BLEND;
	return 1;
}
