#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_CBMB() {
	Identifier = "DEFAULT_PT_CBMB";
	Name = "CBMB";
	Colour = PIXPACK(0x4AD998);
	MenuVisible = 1;
	MenuSection = SC_FORCE;
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 30;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;
	DefaultProperties.tmp2 = 0;

	HeatConduct = 29;
	Description = "Chrono bomb. Sticks to the first object it touches then produces a strong time dilation field.";

	Properties = TYPE_PART | PROP_NO_TIME;

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

void Element_CBMB_time_dilation(Simulation *sim, int x, int y, int radius, int val) {
	x = x / CELL;
	y = y / CELL;

	for (int dx = -radius; dx <= radius; ++dx) 
		for (int dy = -radius; dy <= radius; ++dy) {
			float r = sqrtf(dx * dx + dy * dy);
			if (r <= radius &&
					x + dx >= 0 && x + dx < XRES / CELL && y + dy >= 0 && y + dy < YRES / CELL) {
				float target = val * (1 - r / radius);
				if (fabs(target) > fabs(sim->time_dilation[y + dy][x + dx]))
					sim->time_dilation[y + dy][x + dx] = target;
			}
		}
}

static int update(UPDATE_FUNC_ARGS) {
	// tmp2 is used for detonation timing
	if (parts[i].tmp2 == 1) {
		sim->kill_part(i);
		return 1;
	}
	if (parts[i].tmp2 > 1)
		--parts[i].tmp2;

	int rx,ry,r;
	if (parts[i].tmp2 <= 0) {
		for (rx = -1; rx < 2; rx++)
			for (ry = -1; ry < 2; ry++) {
				if (BOUNDS_CHECK) {
					r = pmap[y+ry][x+rx];
					if(!r)
						continue;
					if (TYP(r)!=PT_BOMB &&
					    TYP(r)!=PT_CLNE && TYP(r)!=PT_PCLN &&
					    TYP(r)!=PT_DMND && TYP(r)!=PT_CBMB &&
						TYP(r)!=PT_FILL) {
						parts[i].tmp2 = 250;
						break;
					}
					// Chain triggering
					else if (TYP(r) == PT_CBMB && parts[ID(r)].tmp2 > 0)
						parts[i].tmp2 = parts[ID(r)].tmp2;
				}
			}
	}

	// Detonation
	if (parts[i].tmp2 > 50) {
		Element_CBMB_time_dilation(sim, x, y, 10, -8);
		parts[i].vx = parts[i].vy = 0;
	} 
	else if (parts[i].tmp2 >= 1) {
		Element_CBMB_time_dilation(sim, x, y, 10, 4);
		parts[i].vx = parts[i].vy = 0;
	}

	// For some reason this is needed for graphics, this isn't calculated
	// Stolen from PRTI
	int orbd[4] = {0, 0, 0, 0};	// Orbital distances
	int orbl[4] = {0, 0, 0, 0};	// Orbital locations
	if (!sim->parts[i].life) parts[i].life = RNG::Ref().gen();
	if (!sim->parts[i].ctype) parts[i].ctype = RNG::Ref().gen();
	sim->orbitalparts_get(parts[i].life, parts[i].ctype, orbd, orbl);
	for (int r = 0; r < 4; r++) {
		if (orbd[r]>1) {
			orbd[r] -= 12;
			if (orbd[r]<1) {
				orbd[r] = RNG::Ref().between(128, 255);
				orbl[r] = RNG::Ref().between(0, 254);
			} else {
				orbl[r] += 2;
				orbl[r] = orbl[r]%255;
			}
		} else {
			orbd[r] = RNG::Ref().between(128, 255);
			orbl[r] = RNG::Ref().between(0, 254);
		}
	}
	sim->orbitalparts_set(&parts[i].life, &parts[i].ctype, orbd, orbl);

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*firea = 8;

	if(cpart->tmp2 > 50) {
		*firer = 74;
		*fireg = 217;
		*fireb = 152;
        *pixel_mode |= EFFECT_GRAVIN;
	}
    else if (cpart->tmp2 >= 1) {  // Invert colors in 2nd stage
		*firer = 181;
		*fireg = 38;
		*fireb = 103;
		*colr = *firer, *colg = *fireg, *colb = *fireb;
        *pixel_mode |= EFFECT_GRAVOUT;
	} else {
		*pixel_mode |= PMODE_SPARK;
	}

	*pixel_mode &= ~PMODE;
	*pixel_mode |= PMODE_ADD;
	return 0;
}
