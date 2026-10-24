#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_SHRD() {
	Identifier = "DEFAULT_PT_SHRD";
	Name = "SHRD";
	Colour = PIXPACK(0x4A443D);
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
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 251;
	Description = "Shredder. Breaks solids and certain powders when they enter.";

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

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * life - is powered?
	 * tmp  - Used for animation
	 */
	if (parts[i].life)
		parts[i].tmp = (parts[i].tmp + 1) % 5;

	int rx, ry, r, rt;
	for (rx = -1; rx <= 1; ++rx)
	for (ry = -1; ry <= 1; ++ry)
		if (BOUNDS_CHECK) {
			r = pmap[y + ry][x + rx];
			if (!r || TYP(r) == PT_SHRD) continue;
			rt = TYP(r);

			// Toggle on off
			if (rt == PT_SPRK) {
				if (parts[ID(r)].ctype == PT_PSCN) {
					PropertyValue value;
					value.Integer = 10;
					sim->flood_prop(x, y, offsetof(Particle, life), value, StructProperty::Integer);
				}
				else if (parts[ID(r)].ctype == PT_NSCN) {
					PropertyValue value;
					value.Integer = 0;
					sim->flood_prop(x, y, offsetof(Particle, life), value, StructProperty::Integer);
				}
			}

			// Shred stuff!
			// These elements have existing breaks and dont need
			// to be turned into BRKN
			if (parts[i].life) {
				int change_type = -1;
				switch (rt) {
					case PT_WOOD:
						change_type = PT_SAWD;
						break;
					case PT_COAL:
						change_type = PT_BCOL;
						break;
					case PT_PLNT:
					case PT_VINE:
						change_type = PT_SEED;
						break;
					case PT_BIRD:
					case PT_FISH:
						change_type = PT_BLOD;
						parts[ID(r)].life = 100;
						break;
					case PT_BEE:
					case PT_ANT:
					case PT_SPDR:
						change_type = PT_BCTR;
						break;
					case PT_AERO:
						change_type = PT_GEL;
						break;
					case PT_IRON:
					case PT_METL:
					case PT_MMSH:
					case PT_BMTL:
						change_type = PT_BRMT;
						break;
					case PT_GLAS:
					case PT_FIBR:
						change_type = PT_BGLA;
						break;
					case PT_CRMC:
						change_type = PT_CLST;
						break;
					case PT_RCRT:
						change_type = PT_CNCT;
						break;
					case PT_CNCT:
						change_type = PT_STNE;
						break;
					case PT_WIFI:
					case PT_ARAY:
					case PT_DRAY:
					case PT_CRAY:
					case PT_EMP:
					case PT_SWCH:
						change_type = PT_BREC;
						break;
					case PT_VIBR:
						change_type = PT_BVBR;
						break;
					case PT_FUSE:
						change_type = PT_FSEP;
						break;
					
					// Special elements that pretend to be solid
					case PT_CRBN:
						parts[ID(r)].tmp2 = 0;
						break;
					case PT_ROCK:
						parts[ID(r)].tmp2 = 1;
						break;
				}

				// Can break under high pressure
				if (change_type == -1 && sim->elements[rt].HighPressureTransition > 0)
					change_type = sim->elements[rt].HighPressureTransition;

				if (change_type >= 0)
					sim->part_change_type(ID(r), x + rx, y + ry, change_type);

				// We wish to shred some solids without a broken state
				// so we change it to BRKN (ctype the solid)
				if (change_type == -1) {
					if (sim->elements[rt].Properties & PROP_INDESTRUCTIBLE)
						continue;

					switch (rt) {
						case PT_SAWD:
							sim->part_change_type(ID(r), x + rx, y + ry, PT_BRKN);
							parts[ID(r)].ctype = PT_SAWD;
							break;
						case PT_MONY:
						case PT_PAPR:
							sim->part_change_type(ID(r), x + rx, y + ry, PT_BRKN);
							parts[ID(r)].ctype = PT_PAPR;
							break;
					}

					// Shred conductable solids
					if (TYP(r) != PT_BRKN && sim->elements[rt].Properties & PROP_CONDUCTS &&
							sim->elements[rt].Properties & TYPE_SOLID &&
							rt != PT_NSCN && rt != PT_PSCN) {
						sim->part_change_type(ID(r), x + rx, y + ry, PT_BRKN);
						parts[ID(r)].ctype = rt;
					}
				}
			}
		}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	int r = (nx / 2 + ny / 2 + cpart->tmp) % 5;
	*colr -= r * 10;
	*colg -= r * 10;
	*colb -= r * 10;

	return 0;
}
