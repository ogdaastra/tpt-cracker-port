#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
int Element_FILT_graphics(GRAPHICS_FUNC_ARGS);
void Element_FILT_create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_PFLT() {
	Identifier = "DEFAULT_PT_PFLT";
	Name = "PFLT";
	Colour = PIXPACK(0x000056);
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
	Hardness = 1;
	Weight = 100;

	HeatConduct = 251;
	Description = "Powered filter. Toggle with PSCN / NSCN, use COPR / ZINC to change tmp.";
	DefaultProperties.life = 10;

	Properties = TYPE_SOLID | PROP_NOAMBHEAT;

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
	Create = &Element_FILT_create;
}

static int update(UPDATE_FUNC_ARGS) {
	if (parts[i].tmp2 > 0)
		parts[i].tmp2--;
	if (parts[i].tmp2 < 0)
		parts[i].tmp2 = 0;

	int rx, ry, r;
	for (rx = -2; rx <= 2; ++rx)
	for (ry = -2; ry <= 2; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (!r) continue;

			if (TYP(r) == PT_SPRK) {
				if (parts[ID(r)].ctype == PT_PSCN && parts[i].life == 0) {
					PropertyValue value;
					value.Integer = 10;
					sim->flood_prop(x, y, offsetof(Particle, life), value, StructProperty::Integer);
				}
				else if (parts[ID(r)].ctype == PT_NSCN && parts[i].life != 0) {
					PropertyValue value;
					value.Integer = 0;
					sim->flood_prop(x, y, offsetof(Particle, life), value, StructProperty::Integer);
				}
				else if (parts[i].tmp2 == 0) {
					if (parts[ID(r)].ctype == PT_COPR && parts[ID(r)].life == 3) {
						PropertyValue value;
						value.Integer = parts[i].tmp + 1;
						if (value.Integer > 100)
							value.Integer = 100;
						sim->flood_prop(x, y, offsetof(Particle, tmp), value, StructProperty::Integer);
						value.Integer = 2;
						sim->flood_prop(x, y, offsetof(Particle, tmp2), value, StructProperty::Integer);
					}
					else if (parts[ID(r)].ctype == PT_ZINC && parts[ID(r)].life == 3) {
						PropertyValue value;
						value.Integer = parts[i].tmp - 1;
						if (value.Integer < 0)
							value.Integer = 0;
						sim->flood_prop(x, y, offsetof(Particle, tmp), value, StructProperty::Integer);
						value.Integer = 2;
						sim->flood_prop(x, y, offsetof(Particle, tmp2), value, StructProperty::Integer);
					}
				}
			}
		}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	Element_FILT_graphics(ren, cpart, nx, ny, pixel_mode, cola, colr, colg, colb, firea, firer, fireg, fireb);
	if (cpart->life == 0) {
		*colr *= 0.2f;
		*colg *= 0.2f;
		*colb *= 0.2f;
	}

	return 0;
}
