#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_MONY() {
	Identifier = "DEFAULT_PT_MONY";
	Name = "MONY";
	Colour = PIXPACK(0x0BA132);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_CRACKER2 in TPT Ultimata
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.00f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.0f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 100;

	DefaultProperties.temp = 273.15f;
	HeatConduct = 40;
	Description = "Money. Attracts STKM, converts GOLD, COAL, BCOL and OIL into more MONY";

	Properties = TYPE_SOLID | PROP_DEADLY;

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

int Element_MONY_RuleTable[9][9] = {
	{0,0,0,0,0,0,0,0,0},
	{0,0,0,1,0,0,0,0,0},
	{0,0,1,0,1,0,1,0,0},
	{0,0,1,0,1,0,1,0,0},
	{0,1,1,1,1,1,1,1,0},
	{0,0,1,0,1,0,1,0,0},
	{0,0,1,0,1,0,1,0,0},
	{0,0,0,0,0,1,0,0,0},
	{0,0,0,0,0,0,0,0,0}
};

int Element_MONY_mony[XRES / 9][YRES / 9];

static int update(UPDATE_FUNC_ARGS) {
	// Attract
	if (sim->player.spwn) {
		float angle = atan2(parts[sim->player.stkmID].y - y, parts[sim->player.stkmID].x - x);
		parts[sim->player.stkmID].vx = -2.0f * cos(angle);
		parts[sim->player.stkmID].vy = -2.0f * sin(angle); 
	}
	if (sim->player2.spwn) {
		float angle = atan2(parts[sim->player2.stkmID].y - y, parts[sim->player2.stkmID].x - x);
		parts[sim->player2.stkmID].vx = -2.0f * cos(angle);
		parts[sim->player2.stkmID].vy = -2.0f * sin(angle); 
	}

	int rx, ry, r, rt;
	for (rx = -2; rx <= 2; ++rx)
		for (ry = -2; ry <= 2; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				if (rt == PT_GOLD || rt == PT_OIL || rt == PT_COAL || rt == PT_BCOL)
					sim->part_change_type(ID(r), x + rx, y + ry, PT_MONY);
			}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*pixel_mode |= FIRE_ADD;
	*firer = 255, *fireg = 255, *fireb = 255, *firea = 20;

	// Glint
	if ((unsigned int)(cpart->x + cpart->y) % 100 == ren->sim->timer % 100)
		*colg = *colr = *colb = 255;
	return 0;
}
