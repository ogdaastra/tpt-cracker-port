#include "simulation/ElementCommon.h"

#include "simulation/FixedUtils.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
void Element_RDMD_create(ELEMENT_CREATE_FUNC_ARGS);

#define CELL_SIZE 30

void Element::Element_RDMD() {
	Identifier = "DEFAULT_PT_RDMD";
	Name = "RDMD";
	Colour = PIXPACK(0xD9FBFF);
	MenuVisible = 1;
	MenuSection = SC_SOLIDS;
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
	Hardness = 0;
	Weight = 100;

	HeatConduct = 150;
	Description = "Realistic Diamond. Tough but destroyable. Slightly flammable.";

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 4027.0f + 273.15f;
	HighTemperatureTransition = PT_LAVA;

	FixedMultiplier = 100000000; // 8

	Create = &Element_RDMD_create;
	Update = &update;
	Graphics = &graphics;
}

void Element_RDMD_create(ELEMENT_CREATE_FUNC_ARGS) {
	int x1 = (int)sim->parts[i].x;
	int y1 = (int)sim->parts[i].y;
	int t1 = sqrt(x1 / CELL_SIZE * x1 / CELL_SIZE + y1 / CELL_SIZE * y1 / CELL_SIZE); // "Seed the x y"

	// Constants were randomly chosen, rotate the above point
	float theta = t1 % 13 / 13.0f * 2 * 3.141592f;
	int x2 = cos(theta) * (x1 - XRES / 2) - sin(theta) * (y1 - YRES / 2) + XRES / 2;
	int y2 = sin(theta) * (x1 - XRES / 2) + cos(theta) * (y1 - YRES / 2) + YRES / 2;

	// Recalculate after rotation
	t1 = sqrt(x2 / CELL_SIZE * x2 / CELL_SIZE + y2 / CELL_SIZE * y2 / CELL_SIZE);
	t1 += y2 / CELL_SIZE; // Avoid repeating diagonals
	
	if (x2 % CELL_SIZE < y2 % CELL_SIZE) // Avoid parallelograms - this cuts them into 2 triangles of different color
		t1++;
	
	// Set seed value
	sim->parts[i].tmp3 = TO_FIXED_E((t1 * 55) % 255 / 255.0f, PT_RDMD);
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Props: tmp3: diamond graphics multiplier
	 * Life: on fire if > 0, dies if life == 1
	 */

	// Produce fire
	bool on_fire = false;
	if (parts[i].life > 0 && parts[i].life != 1) {
		parts[i].life--;
		on_fire = true;
	}

	// Die
	if (parts[i].life == 1) {
		sim->part_change_type(i, parts[i].x, parts[i].y, PT_CO2);
		return 0;
	}

	int rx, ry, r, rt;
	for (rx = -1; rx < 2; rx++)
		for (ry = -1; ry < 2; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) {
					if (on_fire && RNG::Ref().chance(1, 10)) {
						int ni = sim->create_part(-1, x + rx, y + ry, PT_FIRE);
						if (ni > -1) {
							parts[ni].life = RNG::Ref().between(0, 50);
							parts[ni].temp = parts[i].temp + 100.0f;
						}
					}
					continue;
				}
				rt = TYP(r);

				if ((rt == PT_FIRE || rt == PT_PLSM || rt == PT_LAVA) && parts[i].life == 0 && RNG::Ref().chance(1, 30))
					parts[i].life = RNG::Ref().between(50, 850);
			}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	float seed = FROM_FIXED(cpart->tmp3, ren->sim->elements[PT_RDMD].FixedMultiplier);
	*colr *= 0.8 + 0.2 * seed;
	*colg *= 0.8 + 0.2 * seed;
	*colb *= 0.9 + 0.1 * seed;

	return 0;
}
