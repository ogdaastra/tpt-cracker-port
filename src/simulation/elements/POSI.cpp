#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
int  Element_ELEC_graphics(GRAPHICS_FUNC_ARGS);
void Element_ELEC_create(ELEMENT_CREATE_FUNC_ARGS);

void Element::Element_POSI() {
	Identifier = "DEFAULT_PT_POSI";
	Name = "POSI";
	Colour = PIXPACK(0xFFEFDF);
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

	DefaultProperties.temp = R_TEMP + 200.0f + 273.15f;
	HeatConduct = 251;
	Description = "Positrons. Annihilates on contact with electrons, removes spark.";

	Properties = TYPE_ENERGY|PROP_LIFE_DEC|PROP_LIFE_KILL_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
	Graphics = &Element_ELEC_graphics;
	Create = &Element_ELEC_create;
}

static int update(UPDATE_FUNC_ARGS) {
	int r, rt, rx, ry, nb, rrx, rry, ni;
	float total_e, vx, vy, angle;

	for (rx=-2; rx<=2; rx++)
	for (ry=-2; ry<=2; ry++)
		if (BOUNDS_CHECK) {
			r = pmap[y+ry][x+rx];
			if (!r)
				r = sim->photons[y + ry][x + rx];
			if (!r) continue;
			rt = TYP(r);

			switch(rt) {
				case PT_GLAS: // Stolen from ELEC
					for (rrx=-1; rrx<=1; rrx++)
					for (rry=-1; rry<=1; rry++)
						if (x+rx+rrx>=0 && y+ry+rry>=0 && x+rx+rrx<XRES && y+ry+rry<YRES) {
							nb = sim->create_part(-1, x+rx+rrx, y+ry+rry, PT_EMBR);
							if (nb!=-1) {
								parts[nb].tmp = 0;
								parts[nb].life = 50;
								parts[nb].temp = parts[i].temp*0.8f;
								parts[nb].vx = RNG::Ref().between(-10, 10);
								parts[nb].vy = RNG::Ref().between(-10, 10);
							}
						}
					sim->kill_part(i);
					return 1;
				case PT_LCRY: // Reset LCRY
					parts[ID(r)].tmp2 = parts[ID(r)].tmp > 1 ? 10 : 0;
					break;
				case PT_PROT: // Repel
					parts[ID(r)].vx += rx;
					parts[ID(r)].vy += ry;
					parts[i].vx -= rx;
					parts[i].vy -= ry;
					break;
				case PT_ELEC: // Turn into 2 PHOT based on kinetic energy
					total_e = sqrtf(parts[i].vx*parts[i].vx+parts[i].vy*parts[i].vy)
						+ sqrtf(parts[ID(r)].vx*parts[ID(r)].vx+parts[ID(r)].vy*parts[ID(r)].vy);
					parts[ID(r)].temp += total_e / 2 * 700.0f;
					parts[i].temp += total_e / 2 * 700.0f;

					angle = RNG::Ref().between(0, 359) / 180.0f * 3.1415f;
					vx = total_e / 2 * cos(angle);
					vy = total_e / 2 * sin(angle);

					parts[ID(r)].vx = vx;
					parts[ID(r)].vy = vy;
					parts[i].vx = -vx;
					parts[i].vy = -vy;

					parts[ID(r)].life = parts[i].life = 1000;
					parts[ID(r)].ctype = parts[i].ctype = 0x3F000000 >> std::min(29, (int)total_e);

					sim->part_change_type(i, x, y, PT_PHOT);
					sim->part_change_type(ID(r), x + rx, y + ry, PT_PHOT);
					
					ni = sim->create_part(-3, x + rx / 2, y + ry / 2, PT_EMBR);
					if (ni > -1) {
						parts[ni].life = 70;
						parts[ni].tmp = 3;
						parts[ni].temp = MAX_TEMP;
					}
					return 1;
					break;
				case PT_BTRY:
					if (RNG::Ref().chance(1, 20))
						sim->part_change_type(ID(r), x+rx, y+ry, PT_BRMT);
					break;
				case PT_NEUT: // Form anti hydrogen
				case PT_APRT:
					sim->part_change_type(ID(r), x+rx, y+ry, PT_ANH2);
					parts[ID(r)].life = 0;
					parts[ID(r)].ctype = 0;
					sim->kill_part(i);
					return 1;
				case PT_DEUT:
					sim->part_change_type(ID(r), x + rx, y + ry, PT_RH2);
					sim->kill_part(i);
					return 1;
				case PT_EXOT: // Negate EXOT
					parts[ID(r)].tmp2 -= 5;
					parts[ID(r)].life = 1000;
					break;
				case PT_SPRK: // Clear spark
					PropertyValue value;
					value.Integer = 0;
					sim->flood_prop(x + rx, y + ry, offsetof(Particle, life), value, StructProperty::Integer);
					break;
				default:
					break;
			}
		}

	return 0;
}
