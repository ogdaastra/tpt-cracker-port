#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_ANH2() {
	Identifier = "DEFAULT_PT_ANH2";
	Name = "ANH2";
	Colour = PIXPACK(0xBD3335);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = 2.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.99f;
	Loss = 0.30f;
	Collision = -0.10f;
	Gravity = 0.00f;
	Diffusion = 3.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 1;

	HeatConduct = 251;
	Description = "Anti-Hydrogen. Annihilates ordinary matter, undergoes fusion at high temperature and pressure.";

	Properties = TYPE_GAS;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int r, rx, ry, rt;
	for (rx = -2; rx < 3; rx++)
	for (ry = -2; ry < 3; ry++)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y+ry][x+rx];
			if (!r) continue;
			rt = TYP(r);

			// Stolen from HYGN
			if (sim->pv[y/CELL][x/CELL] > 45.0f) {
				if (parts[ID(r)].temp > 2273.15)
					continue;
			}
			else {
				if (rt == PT_FIRE) {
					if(parts[ID(r)].tmp & 0x02)
						parts[ID(r)].temp = 3473.0f;
					else
						parts[ID(r)].temp = 2473.15f;
					parts[ID(r)].tmp |= 1;
					int ni = sim->create_part(i, x, y,PT_FIRE);
					if (ni >= 0)
						parts[ni].life = RNG::Ref().between(10, 120);
					parts[i].temp += RNG::Ref().between(0, 99);
					parts[i].tmp |= 1;
					return 1;
				}
				else if ((rt == PT_PLSM && !(parts[ID(r)].tmp & 4)) || (rt == PT_LAVA && parts[ID(r)].ctype != PT_BMTL)) {
					int ni = sim->create_part(i, x, y,PT_FIRE);
					if (ni >= 0)
						parts[ni].life = RNG::Ref().between(10, 120);
					parts[i].temp += RNG::Ref().between(0, 99);
					parts[i].tmp |= 1;
					return 1;
				}

				// Destroy on contact with ordinary matter
				if (!(sim->elements[rt].Properties & PROP_INDESTRUCTIBLE) && rt!=PT_AMTR && 
						rt!=PT_PCLN && rt!=PT_VOID && rt!=PT_BHOL && rt!=PT_NBHL && rt!=PT_PRTI && rt!=PT_PRTO &&
						rt!=PT_ANH2 && rt!=PT_FIRE && rt!=PT_PLSM && rt != PT_PBCN && rt != PT_BCLN) {
					sim->pv[y/CELL][x/CELL] += 10.0f;
					parts[ID(r)].temp += 9000.0f;

					int count = RNG::Ref().between(4, 50);
					for (int j = 0; j < count; j++) {
						int ni = sim->create_part(-3, x, y, PT_PHOT);
						if (ni >= 0) {
							parts[ni].temp = MAX_TEMP;
							parts[ni].life = RNG::Ref().between(0, 299);

							float angle = RNG::Ref().uniform01() * 2.0f * M_PI;
							float v = RNG::Ref().uniform01() * 5.0f;
							parts[ni].vx = v * cosf(angle);
							parts[ni].vy = v * sinf(angle);
						}
					}

					sim->kill_part(ID(r));
					sim->kill_part(i);
					return 1;
				}
			}
		}

	// Stolen from hydrogen
	if (parts[i].temp > 2273.15 && sim->pv[y/CELL][x/CELL] > 50.0f) {
		if (RNG::Ref().chance(1, 5)) {
			int j;
			float temp = parts[i].temp;
			sim->create_part(i,x,y,PT_AMTR);
			parts[i].tmp2 = 1;

			j = sim->create_part(-3,x,y,PT_NEUT);
			if (j>-1)
				parts[j].temp = temp;
			if (RNG::Ref().chance(1, 10)) {
				j = sim->create_part(-3,x,y,PT_POSI);
				if (j>-1)
					parts[j].temp = temp;
			}
			j = sim->create_part(-3,x,y,PT_PHOT);
			if (j>-1) {
				parts[j].ctype = 0x7C0000;
				parts[j].temp = temp;
				parts[j].tmp = 0x1;
			}
			rx = x + RNG::Ref().between(-1, 1), ry = y + RNG::Ref().between(-1, 1), rt = TYP(pmap[ry][rx]);
			if (sim->can_move[PT_PLSM][rt] || rt == PT_ANH2) {
				j = sim->create_part(-3,rx,ry,PT_PLSM);
				if (j>-1) {
					parts[j].temp = temp;
					parts[j].tmp |= 4;
				}
			}
			parts[i].temp = temp + RNG::Ref().between(750, 1249);
			sim->pv[y/CELL][x/CELL] += 30;
			return 1;
		}
	}

	return 0;
}
