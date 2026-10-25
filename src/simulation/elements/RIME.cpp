#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_RIME()
{
	Identifier = "DEFAULT_PT_RIME";
	Name = "RIME";
	Colour = PIXPACK(0xCCCCCC);
	MenuVisible = 1;
	MenuSection = SC_SOLIDS;
	Enabled = 1;

	Advection = 0.00f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.00f;
	Loss = 0.00f;
	Collision = 0.00f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f  * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 30;

	Weight = 100;

	DefaultProperties.temp = -30.0f + 273.15f;
	HeatConduct = 100;
	Description = "Solid, created when steam cools rapidly and goes through sublimation.";

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15f;
	HighTemperatureTransition = ST; // WATR, or ACID when it absorbed GAS (Simulation::UpdateParticles)

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS)
{
	int r, rx, ry;
	for (rx=-1; rx<2; rx++)
		for (ry=-1; ry<2; ry++)
			if (BOUNDS_CHECK && (rx || ry))
			{
				r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if (TYP(r)==PT_SPRK)
				{
					sim->part_change_type(i,x,y,PT_FOG);
					parts[i].life = RNG::Ref().between(60, 119);
				}
				else if (TYP(r)==PT_FOG&&parts[ID(r)].life>0)
				{
					sim->part_change_type(i,x,y,PT_FOG);
					parts[i].life = parts[ID(r)].life;
				}
				else if (TYP(r) == PT_GAS && parts[i].tmp < 10) // absorbs GAS and turns acidic (upstream 56ede6ca)
				{
					sim->kill_part(ID(r));
					parts[i].tmp++;
				}
			}
	return 0;
}
