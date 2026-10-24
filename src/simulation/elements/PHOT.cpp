#include "simulation/ElementCommon.h"

int Element_FIRE_update(UPDATE_FUNC_ARGS);
static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_PHOT()
{
	Identifier = "DEFAULT_PT_PHOT";
	Name = "PHOT";
	Colour = PIXPACK(0xFFFFFF);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 1.00f;
	Loss = 1.00f;
	Collision = -0.99f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = -1;

	DefaultProperties.temp = R_TEMP + 900.0f + 273.15f;
	HeatConduct = 251;
	Description = "Photons. Refracts through glass, scattered by quartz, and color-changed by different elements. Ignites flammable materials.";

	Properties = TYPE_ENERGY|PROP_LIFE_DEC|PROP_LIFE_KILL_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	DefaultProperties.life = 680;
	DefaultProperties.ctype = 0x3FFFFFFF;

	Update = &update;
	Graphics = &graphics;
	Create = &create;
}

static int update(UPDATE_FUNC_ARGS)
{
	int r, rx, ry;
	float rr, rrr;
	if (!(parts[i].ctype&0x3FFFFFFF)) {
		sim->kill_part(i);
		return 1;
	}
	if (parts[i].temp > 506)
		if (RNG::Ref().chance(1, 10))
			Element_FIRE_update(UPDATE_FUNC_SUBCALL_ARGS);

	// PHOT has different code for moving through FIBR
	if (TYP(pmap[y][x]) == PT_FIBR) {
		int max_speed = parts[ID(pmap[y][x])].tmp;
		int vx = isign(parts[i].vx);
		int vy = isign(parts[i].vy);
		int dx = 0, dy = 0;

		// In the special case where we have no velocity just do nothing
		if (vx == 0 && vy == 0) {
			return 0;
		}
		// Freezing fiber
		else if (max_speed == 0) {
			parts[i].vx = parts[i].vy = 0.0f;
			return 0;
		}
		
		while (abs(dx) < max_speed && abs(dy) < max_speed) {
			dx += vx, dy += vy;

			// Bounds check and make sure velocity is not 0
			if (x + dx >= 0 && x + dx < XRES && y + dy >= 0 && y + dy < YRES) {
				int r2 = pmap[y + dy][x + dx];
				if (TYP(r2) != PT_FIBR) {
					std::vector<std::pair<int, int> > branches;
					dx -= vx;
					dy -= vy;

					// Look for nearby FIBR to redirect to
					for (int rx = -1; rx <= 1; ++rx)
					for (int ry = -1; ry <= 1; ++ry)
						if (BOUNDS_CHECK && (rx || ry)) {
							int r2 = pmap[y + dy + ry][x + dx + rx];
							if (r2 && TYP(r2) == PT_FIBR && (rx != -vx || ry != -vy) && (rx == 0 || ry == 0))
								branches.push_back(std::make_pair(rx, ry));
						}

					if (branches.size() == 0) {
						// Avoid stopping at end
						dx = vx * max_speed;
						dy = vy * max_speed;
					}
					if (branches.size() == 1) {
						parts[i].x += dx;
						parts[i].y += dy;
						dx = branches[0].first;
						dy = branches[0].second;
					}
					else if (branches.size() > 1) {
						int j = RNG::Ref().between(0, branches.size() - 1);
						parts[i].x += dx;
						parts[i].y += dy;
						dx = branches[j].first;
						dy = branches[j].second;
					}

					break;
				}
			}
			else { // Out of bounds
				dx = vx;
				dy = vy;
				break;
			}
		}
		parts[i].vx = dx;
		parts[i].vy = dy;
	}
	
	for (rx=-1; rx<2; rx++)
		for (ry=-1; ry<2; ry++)
			if (BOUNDS_CHECK) {
				r = pmap[y+ry][x+rx];
				if (!r)
					continue;
				if (TYP(r)==PT_ISOZ || TYP(r)==PT_ISZS)
				{
					if (RNG::Ref().chance(1, 400))
					{
						parts[i].vx *= 0.90f;
						parts[i].vy *= 0.90f;
						sim->create_part(ID(r), x+rx, y+ry, PT_PHOT);
						rrr = RNG::Ref().between(0, 359) * 3.14159f / 180.0f;
						if (TYP(r) == PT_ISOZ)
							rr = RNG::Ref().between(128, 255) / 127.0f;
						else
							rr = RNG::Ref().between(128, 355) / 127.0f;
						parts[ID(r)].vx = rr*cosf(rrr);
						parts[ID(r)].vy = rr*sinf(rrr);
						sim->pv[y/CELL][x/CELL] -= 15.0f * CFDS;
					}
				}
				else if (TYP(r) == PT_RSST && !ry && !rx) // if on RSST, make it solid
				{
					int ct_under = parts[ID(r)].ctype;
					int tmp_under = parts[ID(r)].tmp;
					// solidify into the ctype set by CLNE, else into RSSS
					if (ct_under > 0 && ct_under < PT_NUM)
					{
						sim->create_part(ID(r), x, y, ct_under);
						if (tmp_under > 0 && tmp_under < PT_NUM)
							parts[ID(r)].ctype = tmp_under;
					}
					else
						sim->part_change_type(ID(r), x, y, PT_RSSS);
					sim->kill_part(i);
					return 1;
				}
				else if((TYP(r) == PT_RDMD || TYP(r) == PT_QRTZ || TYP(r) == PT_PQRT) && !ry && !rx)//if on QRTZ
				{
					float a = RNG::Ref().between(0, 359) * 3.14159f / 180.0f;
					parts[i].vx = 3.0f*cosf(a);
					parts[i].vy = 3.0f*sinf(a);
					if(parts[i].ctype == 0x3FFFFFFF)
						parts[i].ctype = 0x1F << RNG::Ref().between(0, 25);
					if (parts[i].life)
						parts[i].life++; //Delay death
				}
				else if(TYP(r) == PT_BGLA && !ry && !rx)//if on BGLA
				{
					float a = RNG::Ref().between(-50, 50) * 0.001f;
					float rx = cosf(a), ry = sinf(a), vx, vy;
					vx = rx * parts[i].vx + ry * parts[i].vy;
					vy = rx * parts[i].vy - ry * parts[i].vx;
					parts[i].vx = vx;
					parts[i].vy = vy;
				}
				else if (((TYP(r) == PT_FILT) || (parts[ID(r)].life > 0 && TYP(r) == PT_PFLT)) && parts[ID(r)].tmp==9)
				{
					parts[i].vx += ((float)RNG::Ref().between(-500, 500))/1000.0f;
					parts[i].vy += ((float)RNG::Ref().between(-500, 500))/1000.0f;
				}
			}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	int x = 0;
	*colr = *colg = *colb = 0;
	for (x=0; x<12; x++) {
		*colr += (cpart->ctype >> (x+18)) & 1;
		*colb += (cpart->ctype >>  x)     & 1;
	}
	for (x=0; x<12; x++)
		*colg += (cpart->ctype >> (x+9))  & 1;
	x = 624/(*colr+*colg+*colb+1);
	*colr *= x;
	*colg *= x;
	*colb *= x;

	*firea = 100;
	*firer = *colr;
	*fireg = *colg;
	*fireb = *colb;

	*pixel_mode &= ~PMODE_FLAT;
	*pixel_mode |= FIRE_ADD | PMODE_ADD | NO_DECO;
	if (cpart->flags & FLAG_PHOTDECO)
	{
		*pixel_mode &= ~NO_DECO;
	}
	return 0;
}

static void create(ELEMENT_CREATE_FUNC_ARGS)
{
	float a = RNG::Ref().between(0, 7) * 0.78540f;
	sim->parts[i].vx = 3.0f * cosf(a);
	sim->parts[i].vy = 3.0f * sinf(a);
	
	int Element_FILT_interactWavelengths(Particle* cpart, int origWl);
	if (TYP(sim->pmap[y][x]) == PT_FILT || TYP(sim->pmap[y][x]) == PT_PFLT)
		sim->parts[i].ctype = Element_FILT_interactWavelengths(&sim->parts[ID(sim->pmap[y][x])], sim->parts[i].ctype);
}
