// PWHT from Jacob1's Mod (github.com/jacob1/The-Powder-Toy, GPL-3.0), rewritten for this code base.
// While powered (life 10, PSCN on / NSCN off like PUMP), flood fills its temperature into the body of
// particles directly above it. ctype 1-13 flood fills another property instead (see propertyNames).
#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static bool createAllowed(ELEMENT_CREATE_ALLOWED_FUNC_ARGS);

void Element::Element_PWHT()
{
	Identifier = "DEFAULT_PT_PWHT";
	Name = "PWHT";
	Colour = PIXPACK(0x381D7C);
	MenuVisible = 1;
	MenuSection = SC_POWERED;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.97f;
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

	HeatConduct = 0;
	Description = "Powered Heater. Flood fill heats particles above it to its temp. Use only one. (Jacob1's Mod)";

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	DefaultProperties.life = 10;

	Update = &update;
	Graphics = &graphics;
	CreateAllowed = &createAllowed;
}

// ctype -> property that is flood filled; 0 (default) is temp
static const char *const propertyNames[] = {
	"temp", "life", "ctype", "type", "tmp", "tmp2", "vy", "vx", "x", "y", "dcolour", "flags", "tmp3", "tmp4",
};

static int update(UPDATE_FUNC_ARGS)
{
	if (parts[i].life != 10)
	{
		if (parts[i].life > 0)
			parts[i].life--;
		return 0;
	}
	if (y < 1 || !pmap[y-1][x])
		return 0;
	int ctype = parts[i].ctype;
	if (ctype < 0 || ctype >= int(sizeof(propertyNames) / sizeof(propertyNames[0])))
	{
		parts[i].ctype = 0;
		return 0;
	}
	for (auto &prop : Particle::GetProperties())
	{
		if (prop.Name != propertyNames[ctype])
			continue;
		PropertyValue value;
		if (prop.Type == StructProperty::Float)
			value.Float = parts[i].temp;
		else if (prop.Type == StructProperty::UInteger)
			value.UInteger = parts[i].dcolour;
		else
			value.Integer = parts[i].tmp;
		sim->flood_prop(x, y - 1, prop.Offset, value, prop.Type);
		break;
	}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	int lifemod = ((cpart->life > 10 ? 10 : cpart->life) * 19);
	*colb += lifemod;
	return 0;
}

static bool createAllowed(ELEMENT_CREATE_ALLOWED_FUNC_ARGS)
{
	return (y > 0 && TYP(sim->pmap[y-1][x]) != PT_PWHT) && (y < YRES-1 && TYP(sim->pmap[y+1][x]) != PT_PWHT);
}
