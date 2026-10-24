// BUTN from Jacob1's Mod (github.com/jacob1/The-Powder-Toy, GPL-3.0), rewritten for this code base.
// Turn it on with PSCN (off with NSCN); while on, drawing on it with any element sparks the
// conductors touching it, like a spark sign.
#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static bool ctypeDraw(CTYPEDRAW_FUNC_ARGS);

void Element::Element_BUTN()
{
	Identifier = "DEFAULT_PT_BUTN";
	Name = "BUTN";
	Colour = PIXPACK(0x005254);
	MenuVisible = 1;
	MenuSection = SC_POWERED;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 100;

	HeatConduct = 251;
	Description = "Button. Can be sparked just by clicking on it, but only when turned on. (Jacob1's Mod)";

	Properties = TYPE_SOLID;

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
	CtypeDraw = &ctypeDraw;
}

static int update(UPDATE_FUNC_ARGS)
{
	// powered state spreads through connected BUTN, as with PUMP
	if (parts[i].life != 10)
	{
		if (parts[i].life > 0)
			parts[i].life--;
		return 0;
	}
	for (int rx = -1; rx <= 1; rx++)
		for (int ry = -1; ry <= 1; ry++)
			if (BOUNDS_CHECK && (rx || ry))
			{
				int r = pmap[y+ry][x+rx];
				if (TYP(r) == PT_BUTN)
				{
					if (parts[ID(r)].life < 10 && parts[ID(r)].life > 0)
						parts[i].life = 9;
					else if (parts[ID(r)].life == 0)
						parts[ID(r)].life = 10;
				}
			}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	if (cpart->life >= 10)
	{
		*colr = 19;
		*colg = 229;
		*colb = 233;
		*pixel_mode |= PMODE_GLOW;
	}
	return 0;
}

static bool ctypeDraw(CTYPEDRAW_FUNC_ARGS)
{
	auto &part = sim->parts[i];
	if (part.life != 10 || t == PT_BUTN)
		return true;
	int x = int(part.x + 0.5f), y = int(part.y + 0.5f);
	for (int rx = -1; rx <= 1; rx++)
		for (int ry = -1; ry <= 1; ry++)
		{
			if (!(rx || ry) || x + rx < 0 || y + ry < 0 || x + rx >= XRES || y + ry >= YRES)
				continue;
			int r = sim->pmap[y+ry][x+rx];
			if (r && TYP(r) != PT_BUTN && (sim->elements[TYP(r)].Properties & PROP_CONDUCTS) && !sim->parts[ID(r)].life)
				sim->create_part(ID(r), x + rx, y + ry, PT_SPRK);
		}
	return true;
}
