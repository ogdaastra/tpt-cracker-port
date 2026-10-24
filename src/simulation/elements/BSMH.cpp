#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void create_crystal_at_point(Simulation *sim, int x, int y, int tmp2, float color) {
	if (y < 0 || y >= YRES) return;
	if (x < 0 || x >= XRES) return;

	int r = sim->pmap[y][x];
	if ((r && TYP(r) != PT_BSMH) || (TYP(r) == PT_BSMH && sim->parts[ID(r)].tmp > 1))
		return;
	int id = (TYP(r) == PT_BSMH && sim->parts[ID(r)].tmp <= 1) ?
		ID(r) : sim->create_part(-1, x, y, PT_BSMH);
	if (id > -1) {
		sim->parts[id].tmp3 = tmp2 % 2;
		sim->parts[id].tmp = 1;
		sim->parts[id].ctype = 0;
		sim->parts[id].tmp4 = color;
	}
}

const std::vector<int> BSMH_COLORS({
	0xd3cc71,
	0xd96280,
	0x39ec9b,
	0xe77da4,
	0x3dd9ef,
	0xe0f2bb
});

void Element::Element_BSMH() {
	Identifier = "DEFAULT_PT_BSMH";
	Name = "BSMH";
	Colour = PIXPACK(0xD0DBDB);
	MenuVisible = 1;
	MenuSection = SC_METALS; // SC_SOLIDS before the menu split
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 1;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 25;
	Description = "Bismuth. Forms square crystals when melted then cooled.";

	Properties = TYPE_SOLID | PROP_CONDUCTS | PROP_LIFE_DEC;
	DefaultProperties.ctype = 1;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = 5.0f;
	HighPressureTransition = PT_BRMT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 544.7f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * ctype = 1 is pure non-crystallized bismuth, otherwise crystal
	 * tmp
	 * 	 0 - Not set yet
	 * 	 1 - Inert crystal
	 * 	 >1 - Growing crystal, max crystal size
	 * tmp3
	 * 	 Used for setting "dark" color bands
	 * tmp4
	 * 	 Used to store color varient
	 * tmp2
	 * 	 Crystal growing size, decrements as size grows
	 */

	// Just cooled, set crystal growth state
	if (parts[i].ctype == 0 && parts[i].tmp == 0) {
		parts[i].tmp = RNG::Ref().chance(1, 30) ? RNG::Ref().between(2, 14) : 1; // 1 / 30 chance to be a grow start location
		parts[i].tmp4 = BSMH_COLORS[RNG::Ref().between(0, BSMH_COLORS.size() - 1)];
	}

	// Grow crystal if hot enough and less than max size (20)
	if (sim->timer % 3 == 0 && parts[i].temp > 400.0f && parts[i].tmp > 1 && parts[i].tmp2 < parts[i].tmp) {
		// Top line
		if (y - parts[i].tmp2 >= 0) {
			for (int x2 = x -parts[i].tmp2; x2 <= x + parts[i].tmp2; x2++)
				create_crystal_at_point(sim, x2, y - parts[i].tmp2, parts[i].tmp2, parts[i].tmp4);
		}
		// Bottom line
		if (y + parts[i].tmp2 < YRES) {
			for (int x2 = x -parts[i].tmp2; x2 <= x + parts[i].tmp2; x2++)
				create_crystal_at_point(sim, x2, y + parts[i].tmp2, parts[i].tmp2, parts[i].tmp4);
		}
		// Left line
		if (x - parts[i].tmp2 >= 0) {
			for (int y2 = y -parts[i].tmp2; y2 <= y + parts[i].tmp2; y2++)
				create_crystal_at_point(sim, x - parts[i].tmp2, y2, parts[i].tmp2, parts[i].tmp4);
		}
		// Right line
		if (x + parts[i].tmp2 < XRES) {
			for (int y2 = y -parts[i].tmp2; y2 <= y + parts[i].tmp2; y2++)
				create_crystal_at_point(sim, x + parts[i].tmp2, y2, parts[i].tmp2, parts[i].tmp4);
		}
		parts[i].tmp2++;

		if (RNG::Ref().chance(1, 3)) // Randomly change color again
			parts[i].tmp4 = BSMH_COLORS[RNG::Ref().between(0, BSMH_COLORS.size() - 1)];
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->tmp4) {
		*colr = PIXR(cpart->tmp4);
		*colg = PIXG(cpart->tmp4);
		*colb = PIXB(cpart->tmp4);
	}
	if (cpart->tmp3) {
		*colr *= 0.5f;
		*colg *= 0.5f;
		*colb *= 0.5f;
	}
	return 0;
}
