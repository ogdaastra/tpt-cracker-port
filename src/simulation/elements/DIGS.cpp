// From Cracker1000's Mod, newer than v55.1 (github.com/cracker1000/The-Powder-Toy, branch Bledge-2024),
// adapted to this TPT 97.0 code base. GPL-3.0.
#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_DIGS()
{
	Identifier = "DEFAULT_PT_DIGS";
	Name = "DIGS";
	Colour = PIXPACK(0xFFFFFF);
	MenuVisible = 1;
	MenuSection = SC_POWERED;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.00f;
	Loss = 0.00f;
	Collision = -0.1f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 91;

	HeatConduct = 255;
	Description = "Digital sign. Powered element, use with PSCN, NSCN and INST. tmp changes modes. (Read wiki for more info.)";
	DefaultProperties.temp = 274.15f;
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
	Create = &create;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS)
{
	if (parts[i].tmp > 5 || parts[i].tmp < 1)
		parts[i].tmp = 1;
	
	if (parts[i].tmp == 5 && parts[i].life == 10)
	{
		parts[i].tmp4 += 1;
        if (parts[i].tmp4 > 9999)
        parts[i].tmp4 = 0;		
	}
	for (auto rx = -2; rx < 3; rx++)
		for (auto ry = -2; ry < 3; ry++)
			if (rx || ry)
			{
				if (!BOUNDS_CHECK)
					continue;
				auto r = pmap[y + ry][x + rx];
				if (!r)
					continue;
			if (sim->parts_avg(i, ID(r), PT_INSL) != PT_INSL)
			{
				//Activation and deactivation part
				if (parts[ID(r)].type == PT_SPRK && parts[ID(r)].ctype == PT_PSCN && parts[ID(r)].life < 3)
				{
					parts[i].life = 10;
				}
				else if (parts[ID(r)].type == PT_SPRK && parts[ID(r)].ctype == PT_NSCN && parts[ID(r)].life < 3)
				{
					parts[i].life = 0;
				}
				else if (parts[ID(r)].type == PT_SPRK && parts[ID(r)].ctype == PT_INST && parts[ID(r)].life > 2)
				{
					parts[i].tmp += 1;
				}
				// Properties copy
				if (parts[i].life == 10)
				{
					parts[i].tmp2 = int(sim->pv[y/CELL][x/CELL]); //Pressure
					parts[i].tmp3 = int(sim->gravp[(y/CELL)*(XRES/CELL)+(x/CELL)]); //Gravity
				}
			}
			}
	return 0;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	sim->parts[i].dcolour = 0xFFFF0000;
	sim->parts[i].tmp = 1;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{ 
	*colr = ((cpart->dcolour >> 16) & 0xFF);
	*colg = ((cpart->dcolour >> 8) & 0xFF);
	*colb = ((cpart->dcolour) & 0xFF);
	if (cpart->life == 10) //Active state
	{
	if (cpart->tmp == 1) // Temp. [1]
	{
	ren->drawtext((int)(cpart->x-8), (int)(cpart->y - 10), String::Build(int(roundf(cpart->temp-273.15f))), *colr, *colg, *colb, 255);
	}
	else if (cpart->tmp == 2) //Pressure [2]
	{
	ren->drawtext((int)(cpart->x-2), (int)(cpart->y - 10), String::Build(cpart->tmp2), *colr, *colg, *colb, 255);
	}
	else if (cpart->tmp == 3) //Gravity [3]
	{
	ren->drawtext((int)(cpart->x-2), (int)(cpart->y - 10), String::Build(cpart->tmp3), *colr, *colg, *colb, 255);
	}
	else if (cpart->tmp == 4) //Crack Mod [4]
	{
	ren->fillrect((int)(cpart->x - 22), (int)(cpart->y-13), 51, 13, *colr, *colg, *colb, 50);
	ren->drawrect((int)(cpart->x - 22), (int)(cpart->y-13), 51, 13, *colr, *colg, *colb, 255);
	ren->drawtext((int)(cpart->x-20), (int)(cpart->y - 10), "Crack Mod", *colr, *colg, *colb, 255);
	}
	else if (cpart->tmp == 5) //Timer [5]
	{
	ren->drawtext((int)(cpart->x-4), (int)(cpart->y - 10), String::Build(cpart->tmp4), *colr, *colg, *colb, 255);
	}
	}
	return 0;
}
