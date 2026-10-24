#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int frozen_graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_FLRN() {
	Identifier = "DEFAULT_PT_FLRN";
	Name = "FLRN";
	Colour = PIXPACK(0xC7A75B);
	MenuVisible = 1;
	MenuSection = SC_GAS;
	Enabled = 1;

	Advection = 0.8f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.4f;
	Loss = 0.70f;
	Collision = -0.1f;
	Gravity = 0.1f;
	Diffusion = 0.6f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 1;

	HeatConduct = 100;
	Description = "Flourine. Reacts violently with almost anything.";

	Properties = TYPE_GAS | PROP_DEADLY | PROP_NEUTPASS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = 273.15f - 188.1f;
	LowTemperatureTransition = PT_LQUD;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
	FrozenGraphics = &frozen_graphics;

	MeltingPoint = 273.15f - 219.6f;
	BoilingPoint = 273.15f - 188.1f;
}

static int update(UPDATE_FUNC_ARGS) {
	for (int rx = -1; rx <= 1; rx++)
	for (int ry = -1; ry <= 1; ry++)
		if (BOUNDS_CHECK && (rx || ry)) {
			int r = pmap[y + ry][x + rx];
			if (!r || TYP(r) == PT_FLRN) {
				if (parts[i].life && sim->pmap_count[y + ry][x + rx] < 3) {
					int j = sim->create_part(-3, x + rx, y + ry, PT_FIRE);
					if (j > -1) {
						parts[j].temp = parts[i].temp + 500.0f;
						parts[j].life = RNG::Ref().between(10, 200);
					}
				}
				continue;
			}
			int rt = TYP(r);

			// Doesn't react with noble gases
			if (rt == PT_NEON || rt == PT_HELM || rt == PT_NBLE || rt == PT_RADN)
				continue;
			// Ignore indestructible elements
			if (sim->elements[rt].Properties & PROP_INDESTRUCTIBLE)
				continue;
			// Ignore clone
			if (rt == PT_CLNE || rt == PT_BCLN || rt == PT_PBCN || rt == PT_PCLN)
				continue;
			// Other special
			if (rt == PT_VOID || rt == PT_BHOL || rt == PT_PVOD || rt == PT_PRTI || rt == PT_PRTO ||
				rt == PT_WHOL || rt == PT_NBHL || rt == PT_NWHL)
				continue;
			// Ignore these:
			if (rt == PT_FIRE || rt == PT_DFLM || rt == PT_PLSM || rt == PT_FREZ || rt == PT_FRZZ || rt == PT_HOLY)
				continue;

			// FLRN + WATR = ACID
			if (sim->elements[rt].Properties & PROP_WATER) {
				parts[i].life = 100;
				parts[ID(r)].life = 100;
				sim->part_change_type(i, x, y, PT_ACID);
				sim->part_change_type(ID(r), x + rx, y + ry, PT_ACID);
				return 1;
			}

			if (parts[i].temp < 200.0f + 273.15f && !sim->elements[rt].Flammable)
				continue;

			// Burn anything else
			parts[i].life = 10;
			if (rt == PT_LAVA || RNG::Ref().chance(1, 500)) {
				parts[ID(r)].life = RNG::Ref().chance(20, 200);
				parts[ID(r)].temp += 500.0f;
				sim->part_change_type(ID(r), x + rx, y + ry, PT_FIRE);
			}
			if (RNG::Ref().chance(1, 1500)) {
				sim->kill_part(i);
				return 1;
			}
		}

	return 0;
}

static int frozen_graphics(GRAPHICS_FUNC_ARGS) {
	*colr = 0xE0;
	*colg = 0xC7;
	*colb = 0x8B;
	return 1;
}
