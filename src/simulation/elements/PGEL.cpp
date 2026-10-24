#include "simulation/ElementCommon.h"
#include "simulation/mvsd/movingsolids.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_PGEL() {
	Identifier = "DEFAULT_PT_PGEL";
	Name = "RGEL";
	Colour = PIXPACK(0x00A0FF);
	MenuVisible = 1;
	MenuSection = SC_LIQUID;
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f  * CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 100;

	DefaultProperties.temp = R_TEMP - 2.0f + 273.15f;

	HeatConduct = 29;
	Description = "Repulsion Gel. Stains solids, washes with water. Very bouncy.";

	Properties = TYPE_LIQUID | PROP_LIFE_DEC | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, rt, r;
	for (rx = -2; rx < 3; rx++)
		for (ry = -2; ry < 3; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				if (parts[i].tmp) { // Solid goo
					parts[i].vx = 0;
					parts[i].vy = 0;
				}
				
				// Delete self after while if liquid
				if (parts[i].tmp == 0 && RNG::Ref().chance(1, 2000)){
					sim->part_change_type(i, parts[i].x, parts[i].y, PT_NONE);
				}

				// Melt when too hot
				if (parts[i].temp > 7000.0f) {
					sim->part_change_type(i, parts[i].x, parts[i].y, PT_LAVA);
					return 0;
				}

				// Washing away with water
				if (sim->elements[rt].Properties & PROP_WATER) {
					parts[i].tmp = 0;
					sim->part_change_type(i, parts[i].x, parts[i].y, parts[i].ctype);
					return 0;
				}

				// Stain with the gel. Basically, don't stain any of the elements below
				else if (parts[i].tmp == 0 && 
							!(sim->elements[rt].Properties & PROP_VEHICLE) &&
							rt != PT_BRAY && rt != PT_GEL && rt != PT_PRTI &&
							rt != PT_BCLN  && rt != PT_PCLN && !(sim->elements[rt].Properties & PROP_INDESTRUCTIBLE) &&
							rt != PT_PRTO && !(sim->elements[rt].Properties & TYPE_LIQUID) &&
							!(sim->elements[rt].Properties & TYPE_GAS) && rt != PT_STKM && rt != PT_STKM2 &&
							rt != PT_FIGH && rt != PT_PGEL && rt != PT_MVSD && rt != PT_FFLD) {
					parts[i].tmp = 1;
					parts[ID(r)].ctype = rt;
					sim->part_change_type(ID(r), parts[ID(r)].x, parts[ID(r)].y, PT_PGEL);
					sim->part_change_type(i, parts[i].x, parts[i].y, parts[i].ctype);
				}

				// Stain MVSD by changing ctype, but only in liquid form
				else if (rt == PT_MVSD && parts[i].tmp == 0 && parts[ID(r)].ctype != PT_PGEL) {
					parts[ID(r)].life = parts[ID(r)].ctype;
					parts[ID(r)].ctype = PT_PGEL;
				}

				// If we're not altering another gel particle we can apply
				// what we need to bounce the particle
				if (parts[i].tmp >= 1 && rt != PT_PGEL) {
					if (rt == PT_STKM || rt == PT_STKM2 || rt == PT_FIGH){
						switch (sim->gravityMode){ // Bouncing for stkm, in diffrent gravity modes
						default:
						case 0:
							parts[ID(r)].vy = -5;
							break;
						case 1:
							parts[ID(r)].vx *= -1.01;
							parts[ID(r)].vy *= -1.01;
							break;
						case 2:{
							parts[ID(r)].vx *= -1.01;
							parts[ID(r)].vy *= -1.01;
							break;
						}
						}
					}
					// Moving solids already bounce, just flag it to bounce higher
					else if (rt == PT_MVSD && parts[ID(r)].tmp2 > 0 && MOVINGSOLID::solids.count(parts[ID(r)].tmp2) > 0) {
						MOVINGSOLID::solids[parts[ID(r)].tmp2].do_big_bounce();
					}
					else {
						parts[ID(r)].vx *= -1.05;
						parts[ID(r)].vy *= -1.05;
					}
				}
			}
		
	return 0;
}
