// EXPL from Jacob1's Mod (github.com/jacob1/The-Powder-Toy, GPL-3.0), rewritten for this code base.
// The explosion itself (FLAG_EXPLODE) is handled in Simulation::UpdateParticles.
#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_EXPL()
{
	Identifier = "DEFAULT_PT_EXPL";
	Name = "EXPL";
	Colour = PIXPACK(0xFEA713);
	MenuVisible = 1;
	MenuSection = SC_EXPLOSIVE;
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.10f * CFDS;
	AirLoss = 0.96f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 99;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
	HeatConduct = 29;
	Description = "Explosion, causes everything it touches to explode. (Jacob1's Mod)";

	Properties = TYPE_PART|PROP_SPARKSETTLE|PROP_INDESTRUCTIBLE;

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
	for (int rx = -1; rx <= 1; rx++)
		for (int ry = -1; ry <= 1; ry++)
			if (BOUNDS_CHECK && (rx || ry))
			{
				int r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if (!(sim->elements[TYP(r)].Properties & PROP_INDESTRUCTIBLE) && TYP(r) != PT_EMBR)
					parts[ID(r)].flags |= FLAG_EXPLODE;
			}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	*pixel_mode |= PMODE_FLARE;
	return 0;
}
