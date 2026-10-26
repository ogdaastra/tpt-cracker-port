#include "simulation/ElementCommon.h"
#include "simulation/mvsd/movingsolids.h"

#include "simulation/FixedUtils.h"

#include <iostream>

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

namespace MVSD {
	void reset(int x, int y, Particle *parts, GridView<int> pmap, int state_id) {
		if (x < 0 || y < 0 || x > XRES || y > YRES)
        	return;
		int id = ID(pmap[y][x]);
		if (parts[id].type != PT_MVSD || parts[id].tmp2 != state_id)
        	return;
		parts[id].tmp2 = 0;

		reset(x - 1, y, parts, pmap, state_id);
		reset(x + 1, y, parts, pmap, state_id);
		reset(x, y - 1, parts, pmap, state_id);
		reset(x, y + 1, parts, pmap, state_id);
	}
}

void Element::Element_MVSD() {
	Identifier = "DEFAULT_PT_MVSD";
	Name = "MVSD";
	Colour = PIXPACK(0xDB3030);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL;
	Enabled = 1;

	HeatConduct = 255;
	Weight = 100;
	Description = "Moving Solid. Mimics its ctype.";

	Properties = TYPE_SOLID;

	FixedMultiplier = 10000; // 4

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Moving solid setup: moving solids are grouped by their ID, which is defined by tmp2
	 * If tmp2 is 0 the particle will automatically begin a floodfill to detect other MVSD that
	 * belongs to this moving solid, and group any that are not part of this moving solid group
	 * 
	 * If flags = 1, solid should be reconstructed (Edit was made)
	 * 
	 * life stores previous ctype (If it was stained by portal gel)
	 * 
	 * tmp3 and tmp4 store the solid's velocity for updating when a solid is cut
	 * into 2 solids
	 */
	if (parts[i].tmp2 == 0)
		MOVINGSOLID::create_moving_solid(parts, pmap, i);

	bool valid_ctype = parts[i].ctype > 0 && parts[i].ctype < PT_NUM && sim->elements[parts[i].ctype].Enabled;
	int px = (int)(parts[i].x + 0.5);
	int py = (int)(parts[i].y + 0.5);

	/** 
	 * Failed to find group, flood fill current state id to 0 and
	 * recreate the solid. This happens sometimes during undo-redo operations
	 * (maybe redo/undo changes particle ids?) Also if flags = 1 redo, there
	 * is a chance the solid was cut
	 */
	if (MOVINGSOLID::solids.count(parts[i].tmp2) == 0 || parts[i].flags == 1) {
		MVSD::reset(px, py, parts, pmap, parts[i].tmp2);
		MOVINGSOLID::create_moving_solid(parts, pmap, i);

		// If the solid was cut, we'll need to set its velocity to the original
		if (parts[i].flags == 1) {
			MOVINGSOLID::solids[parts[i].tmp2].set_velocity(FROM_FIXED_E(parts[i].tmp3, PT_MVSD), FROM_FIXED_E(parts[i].tmp4, PT_MVSD));
			parts[i].tmp3 = parts[i].tmp4 = 0;
		}

		parts[i].flags = 0;
		return 0;
	}

	/* Ctype is not a solid or portal gel */
	if (!valid_ctype)
		parts[i].ctype = 0;
	else if (!(sim->elements[parts[i].ctype].Properties & TYPE_SOLID) && parts[i].ctype != PT_PGEL)
		parts[i].ctype = 0;

	/* Basic ctype mimicing */
	if (valid_ctype) {
		// Flammability mimic
		if ((sim->elements[parts[i].ctype].Explosive & 2) && sim->pv[y / CELL][x / CELL] > 2.5f) {
			parts[i].life = RNG::Ref().between(180, 259);
			parts[i].temp = restrict_flt(sim->elements[PT_FIRE].DefaultProperties.temp + (sim->elements[parts[i].ctype].Flammable / 2), MIN_TEMP, MAX_TEMP);
			sim->part_change_type(i, x, y, PT_FIRE);
			sim->pv[y / CELL][x / CELL] += 0.25f * CFDS;
			return 0;
		}

		int change_type = -1;
		bool change_ctype = false;

		// Melt if ctype allows it
		if (sim->elements[parts[i].ctype].HighTemperatureTransition != NT
				&& parts[i].temp > sim->elements[parts[i].ctype].HighTemperature) {
			change_type = sim->elements[parts[i].ctype].HighTemperatureTransition;
			change_ctype = true;
		}

		// Shatter depending on pressure transition
		else if (sim->elements[parts[i].ctype].HighPressureTransition != NT
				&& -1 < sim->elements[parts[i].ctype].HighPressure
				&& sim->pv[y/CELL][x/CELL] > sim->elements[parts[i].ctype].HighPressure) {
			change_type = sim->elements[parts[i].ctype].HighPressureTransition;
		}
		else if (sim->elements[parts[i].ctype].LowPressureTransition != NT
				&& -1 < sim->elements[parts[i].ctype].LowPressure
				&& sim->pv[y/CELL][x/CELL] < sim->elements[parts[i].ctype].LowPressure) {
			change_type = sim->elements[parts[i].ctype].LowPressureTransition;
		}

		// We have to change into something
		if (change_type > -1) {
			// Ice transitions are weird
			if (parts[i].ctype == PT_ICEI)
				change_type = PT_ICEI;

			int temp = sim->create_part(-3, x, y, change_type);

			if (temp > -1) {
				// Ice for some reason doesn't default ctype to water
				if (parts[i].ctype == PT_ICEI)
					parts[temp].ctype = PT_WATR;
				else if (change_ctype)
					parts[temp].ctype = parts[i].ctype;

				parts[temp].temp = parts[i].temp;
				if (!(sim->elements[change_type].Properties & TYPE_SOLID)) {
					parts[temp].vx = MOVINGSOLID::solids[parts[i].tmp2].getVX();
					parts[temp].vy = MOVINGSOLID::solids[parts[i].tmp2].getVY();
				}
			}
			sim->kill_part(i);
			return 0;
		}
	}

	int r, rx, ry;

	// Flag if it overlaps
	if (sim->pmap_count[y][x] > 1)
		MOVINGSOLID::solids[parts[i].tmp2].flag_overlap();

	// Should it make pressure on impact?
	MOVINGSOLID::solids[parts[i].tmp2].should_impact_pressure(parts[i].tmp == 0);

	// If self has a velocity somehow update the moving solid
	if (parts[i].vx || parts[i].vy) {
		MOVINGSOLID::solids[parts[i].tmp2].set_velocity(parts[i].vx, parts[i].vy);
		parts[i].vx = parts[i].vy = 0;
	}

	// Check surrounding particles. These are "touching"
	// collisions, as in the solid is flush with another particle
	for (rx=-1; rx<2; rx++)
		for (ry=-1; ry<2; ry++)
			if (BOUNDS_CHECK) {
				// Allow wall collisions
				if (sim->IsWallBlocking(x + rx, y + ry, PT_MVSD))
					MOVINGSOLID::solids[parts[i].tmp2].add_collision(
						MOVINGSOLID::Collision(x + rx, y + ry, MOVINGSOLID::STATIC, px, py, x + rx, y + ry));

				r = pmap[y+ry][x+rx];
				if (!r) continue;
				int rt = TYP(r);

				// Flammability mimic
				if (valid_ctype && (sim->elements[parts[i].ctype].Explosive & 2) && rt == PT_FIRE) {
					parts[i].life = RNG::Ref().between(180, 259);
					parts[i].temp = restrict_flt(sim->elements[PT_FIRE].DefaultProperties.temp + (sim->elements[parts[i].ctype].Flammable / 2), MIN_TEMP, MAX_TEMP);
					sim->part_change_type(i, x, y, PT_FIRE);
					sim->pv[y / CELL][x / CELL] += 0.25f * CFDS;
				}

				// Unstain portal gel
				if (rt == PT_WATR || rt == PT_SLTW || rt == PT_DSTW)
					parts[i].ctype = parts[i].life;

				// Collision with another moving solid
				if (rt == PT_MVSD && parts[ID(r)].tmp2 != parts[i].tmp2) {
					MOVINGSOLID::solids[parts[i].tmp2].add_collision(
						MOVINGSOLID::Collision(x + rx, y + ry, MOVINGSOLID::MOVING, px, py, x + rx, y + ry));
				}

				// General collision (Ignore FILL)
				else if (rt != PT_MVSD && rt != PT_FILL) {
					if (sim->elements[rt].Properties & TYPE_SOLID)
						MOVINGSOLID::solids[parts[i].tmp2].add_collision(
							MOVINGSOLID::Collision(x + rx, y + ry, MOVINGSOLID::STATIC, px, py, x + rx, y + ry));
					else if (sim->elements[rt].Properties & TYPE_PART || rt == PT_PGEL)
						MOVINGSOLID::solids[parts[i].tmp2].add_collision(
							MOVINGSOLID::Collision(x + rx, y + ry, MOVINGSOLID::NONSTATIC, px, py, x + rx, y + ry));
				}
			}

	// Check for phasing into solids
	float vx_ = MOVINGSOLID::solids[parts[i].tmp2].getVX(),
		  vy_ = MOVINGSOLID::solids[parts[i].tmp2].getVY();

	if (vx_ || vy_) {
		float largest = fabs(vx_) > fabs(vy_) ? fabs(vx_) : fabs(vy_);
		if (largest <= 1) // Avoid "division by 0" effect where it ends up scanning the entire map
			return 0;

		float dvx = vx_ / largest, dvy = vy_ / largest;
		float sx = (int)(parts[i].x + 0.5), sy = (int)(parts[i].y + 0.5);
		int px = 0, py = 0;
		int count = 1;

		while (sx >= 0 && sy >= 0 && sx < XRES && sy < YRES && fabs(-dvx + dvx * count) <= fabs(vx_) && fabs(-dvy + dvy * count) <= fabs(vy_)) {
			if (px == (int)round(sy) && py == (int)round(sy))
				goto endloop;

			// Avoid rubberbanding
			if (fabs((int)round(sx) - px) > MOVINGSOLID::MAX_VELOCITY ||
				fabs((int)round(sy) - py) > MOVINGSOLID::MAX_VELOCITY)
				goto endloop;

			r = pmap[(int)round(sy)][(int)round(sx)];
			if (r) {
				if (TYP(r) != PT_MVSD && TYP(r) != PT_FILL && 
						(sim->elements[TYP(r)].Properties & TYPE_PART ||
						 sim->elements[TYP(r)].Properties & TYPE_SOLID)) {
					MOVINGSOLID::solids[parts[i].tmp2].add_collision(
						MOVINGSOLID::Collision((int)sx, (int)sy, 
						sim->elements[TYP(r)].Properties & TYPE_PART ? MOVINGSOLID::NONSTATIC : MOVINGSOLID::STATIC,
						px, py, (int)round(sx), (int)round(sy)));

					int dx = (int)round(sx) - px;
					int dy = (int)round(sy) - py;

					if (dx != 0) dx += dx < 0 ? 1 : -1;
					if (dy != 0) dy += dy < 0 ? 1 : -1;
						
					MOVINGSOLID::solids[parts[i].tmp2].update_delta(dx, dy);
					parts[i].tmp = 1;
					break;
				} 
				else if (parts[ID(r)].tmp2 != parts[i].tmp2) {
					// Collisions with other solids are handled
					// in simulation/mvsd
				}
			}

			endloop:
			px = (int)round(sx); py = (int)round(sy);
			sx += dvx; sy += dvy;
			count++;
		}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Mimic ctype color
	bool valid_ctype = cpart->ctype > 0 && cpart->ctype < PT_NUM && ren->sim->elements[cpart->ctype].Enabled;
	if (valid_ctype) {
		*colr = PIXR(ren->sim->elements[cpart->ctype].Colour);
		*colg = PIXG(ren->sim->elements[cpart->ctype].Colour);
		*colb = PIXB(ren->sim->elements[cpart->ctype].Colour);

		// Stolen HOT_GLOW code
		if (ren->sim->elements[cpart->ctype].Properties & PROP_HOT_GLOW && cpart->temp > (ren->sim->elements[cpart->ctype].HighTemperature - 800.0f)) {
			float gradv = 3.1415 / (2 * ren->sim->elements[cpart->ctype].HighTemperature - ren->sim->elements[cpart->ctype].HighTemperature + 800.0f);
			float caddress = (cpart->temp > ren->sim->elements[cpart->ctype].HighTemperature) ? ren->sim->elements[cpart->ctype].HighTemperature - (ren->sim->elements[cpart->ctype].HighTemperature - 800.0f) : cpart->temp - (ren->sim->elements[cpart->ctype].HighTemperature - 800.0f);
			*colr += sin(gradv * caddress) * 226;
			*colg += sin(gradv * caddress * 4.55 + 3.14) * 34;
			*colb += sin(gradv * caddress * 2.22 + 3.14) * 64;
		}
	}

	return 0;
}
