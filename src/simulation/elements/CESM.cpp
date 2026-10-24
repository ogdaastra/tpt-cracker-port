#include "simulation/ElementCommon.h"

int Element_CESM_update(UPDATE_FUNC_ARGS);
int Element_CESM_graphics(GRAPHICS_FUNC_ARGS);

// See https://powdertoy.co.uk/Discussions/Thread/View.html?Thread=21682

void Element::Element_CESM() {
	Identifier = "DEFAULT_PT_CESM";
	Name = "CESM";
	Colour = PIXPACK(0xD1E765);
	MenuVisible = 1;
	MenuSection = SC_EXPLOSIVE;
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
	Meltable = 50;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 255;
	Description = "Cesium. Reactive alkali metal. Very low melting point.";

	Properties = TYPE_SOLID | PROP_CONDUCTS | PROP_LIFE_DEC;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15f + 28.0f;
	HighTemperatureTransition = PT_LCSM;

	Update = &Element_CESM_update;
	Graphics = &Element_CESM_graphics;
}

int Element_CESM_update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * tmp - Type
	 * 	0 = Default
	 * 	1 = Cesium oxide
	 *  2 = CsAu
	 *  -1 = explosion
	 */

	static int checkCoordsX[] = { -4, 4, 0, 0 };
	static int checkCoordsY[] = { 0, 0, -4, 4 };

	int rx, ry, r, rt;
	int explosion_type = 0;

	// Gold like conduction
	if (!parts[i].life && parts[i].tmp == 2) {
		for(int j = 0; j < 4; j++){
			rx = checkCoordsX[j];
			ry = checkCoordsY[j];
			if (BOUNDS_CHECK) {
				r = pmap[y+ry][x+rx];
				if (!r) continue;
				if (TYP(r) == PT_SPRK && parts[ID(r)].life && parts[ID(r)].life < 4) {
					sim->part_change_type(i, x, y, PT_SPRK);
					parts[i].life = 4;
					parts[i].ctype = PT_CESM;
				}
			}
		}
	}

	// Release O2
	if (parts[i].tmp == 1 && (parts[i].tmp > 500.0f + 273.15f || parts[i].life)) {
		sim->part_change_type(i, x, y, PT_LCSM);
		sim->create_part(-1, x + RNG::Ref().between(-1, 2), y + RNG::Ref().between(-1, 2), PT_O2);
		return 1;
	}

	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) r = sim->photons[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				if (parts[i].tmp != -1) {
					if (parts[i].temp > 273.15f + 1600.0f ||
						(rt == PT_FIRE || rt == PT_PLSM || rt == PT_DFLM ||
						 sim->elements[rt].Properties & PROP_WATER || rt == PT_WTRV)) {
						explosion_type = rt;
						PropertyValue value;
						value.Integer = -1;
						sim->flood_prop(x, y, offsetof(Particle, tmp), value, StructProperty::Integer);
					}
				}
				// PHOT to ELEC
				if (rt == PT_PHOT)
					sim->part_change_type(ID(r), x + rx, y + ry, PT_ELEC);
				// Acid reactions
				else if (rt == PT_ACID || rt == PT_CAUS) {
					if (parts[i].tmp == 1) {
						sim->part_change_type(i, x, y, RNG::Ref().chance(1, 2) ? PT_DSTW : PT_SALT);
						return 1;
					}
					else {
						sim->part_change_type(i, x, y, RNG::Ref().chance(1, 2) ? PT_SALT : PT_H2);
						return 1;
					}
				}
				// Conversion to cesium oxide
				else if (rt == PT_O2) {
					parts[i].tmp = 1;
					sim->kill_part(ID(r));
				}
				// Coversion to CsAu
				else if (rt == PT_GOLD)
					parts[i].tmp = 2;
			}

	// Explosion code
	if (parts[i].tmp == -1) {
		sim->pv[y / CELL][x / CELL] += 2.0f;
		float otemp = parts[i].temp - 273.15f;

		// Stolen from TNT lol
		if (RNG::Ref().chance(1, 2)) {
			if (RNG::Ref().chance(1, 2))
				sim->create_part(-3, x, y, PT_FIRE);
			else {
				sim->create_part(-3, x, y, PT_SMKE);
				parts[i].life = RNG::Ref().between(500, 549);
			}
			parts[i].temp = restrict_flt((MAX_TEMP / 4) + otemp, MIN_TEMP, MAX_TEMP);
		}
		else if (RNG::Ref().chance(1, 10)) {
			sim->create_part(-3, x, y, PT_EMBR);
			parts[i].tmp = 0;
			parts[i].life = 50;
			parts[i].temp = restrict_flt((MAX_TEMP / 3) + otemp, MIN_TEMP, MAX_TEMP);
			parts[i].vx = RNG::Ref().between(-10, 10);
			parts[i].vy = RNG::Ref().between(-10, 10);
		}

		if (parts[i].tmp == 2 &&
			(explosion_type == PT_WATR || explosion_type == PT_WTRV || explosion_type == PT_DSTW)) {
			sim->part_change_type(i, x, y, RNG::Ref().chance(1, 2) ? PT_GOLD : PT_SALT);
		}
		else {
			sim->kill_part(i);
		}
		return 1;
	}

	return 0;
}

int Element_CESM_graphics(GRAPHICS_FUNC_ARGS) {
	// Cesium oxide
	if (cpart->tmp == 1) {
		*colr *= 0.8f;
		*colg *= 0.8f;
		*colb *= 0.8f;
	}

	// CsAu
	else if (cpart->tmp == 2) {
		*colr *= 1.15f;
		*colg *= 1.05f;
	}

	return 0;
}
