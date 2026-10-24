#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_AMTR()
{
	Identifier = "DEFAULT_PT_AMTR";
	Name = "AMTR";
	Colour = PIXPACK(0x808080);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = 0.7f;
	AirDrag = 0.02f * CFDS;
	AirLoss = 0.96f;
	Loss = 0.80f;
	Collision = 0.00f;
	Gravity = 0.10f;
	Diffusion = 1.00f;
	HotAir = 0.0000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 100;

	HeatConduct = 70;
	Description = "Anti-Matter, destroys a majority of particles.";

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
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS)
{
	int r, rx, ry, rt;
	for (rx=-1; rx<2; rx++)
		for (ry=-1; ry<2; ry++)
			if (BOUNDS_CHECK && (rx || ry))
			{
				r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				rt = TYP(r);
				if (!(sim->elements[rt].Properties & PROP_INDESTRUCTIBLE) && rt!=PT_AMTR && 
						rt!=PT_PCLN && rt!=PT_VOID && rt!=PT_BHOL && rt!=PT_NBHL && rt!=PT_PRTI && rt!=PT_PRTO &&
						rt!=PT_ANH2)
				{
					if (parts[i].tmp2 == 1) { // Realistic antimatter explosion
						sim->pv[y/CELL][x/CELL] -= 10.0f;
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
					else { // Default antimatter behavior
						parts[i].life++;
						if (parts[i].life == 4) {
							sim->kill_part(i);
							return 1;
						}
						if (RNG::Ref().chance(1, 10))
							sim->create_part(ID(r), x+rx, y+ry, PT_PHOT);
						else
							sim->kill_part(ID(r));
						sim->pv[y/CELL][x/CELL] -= 2.0f;
					}
				}
			}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->tmp2 == 1) {
		*firer = *fireg = *fireb = 150;
		*firea = 60;
		*pixel_mode |= FIRE_ADD;
	}

	return 0;
}
