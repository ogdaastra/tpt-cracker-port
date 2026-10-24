#include "simulation/ElementCommon.h"
#include <iostream>

static int update(UPDATE_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);

#define BIRD_SEARCH_RANGE 5
#define BIRD_AVOID_RANGE 2
#define MAX_VELOCITY 0.8f
#define ACCELERATION 0.05f
#define AVOID_TEMP 90.0f + 273.15f

#define EDGE_RANGE 10
#define EDGE_PUSH 200.0f

void Element::Element_BIRD() {
	Identifier = "DEFAULT_PT_BIRD";
	Name = "BIRD";
	Colour = PIXPACK(0xB8A174);
	MenuVisible = 1;
	MenuSection = SC_ORGANIC; // SC_ORGANIC in TPT Ultimata; SC_SPECIAL before the menu split
	Enabled = 1;

	Advection = 0.1f;
	AirDrag = 0.000f * CFDS;
	AirLoss = 0.94f;
	Loss = 1.0f;
	Collision = -0.1f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 30;
	Explosive = 1;
	Meltable = 0;
	Hardness = 2;

	Weight = 20;

	HeatConduct = 150;
	Description = "Birds, flies in flocks, eats ants, fish, and seeds. Flammable.";

	Properties = TYPE_PART | PROP_EDIBLE;
	FoodValue = 2;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = 6.0f;
	HighPressureTransition = PT_BLOD;
	LowTemperature = 273.15f - 20.0f;
	LowTemperatureTransition = PT_DUST;
	HighTemperature = 273.15f + 90.0f;
	HighTemperatureTransition = PT_DUST;

	Update = &update;
	Create = &create;
}

static void create(ELEMENT_CREATE_FUNC_ARGS) {
	// Randomize velocity
	float angle = RNG::Ref().uniform01() * 2 * 3.141592f;
	sim->parts[i].vx = cos(angle);
	sim->parts[i].vy = sin(angle);

	// Store spawn location
	sim->parts[i].tmp3 = (int)(sim->parts[i].x + 0.5f);
	sim->parts[i].tmp4 = (int)(sim->parts[i].y + 0.5f);
}

static int update(UPDATE_FUNC_ARGS) {
	/** Properties:
	 * tmp - is perching?
	 * tmp2 - perch timer
	 * life - want to perch?
	 * tmp3 and tmp4: spawn location
	 * 
	 * Behavior:
	 * - Birds flock
	 * - Randomly land onto solids
	 * - Avoid fire, pressure, heat, lava, anything PROP_DEADLY
	 * - Head towards ANT and SEED
	 * - If edge mode is WALL or VOID, avoid edges
	 */
	int rx, ry, r, rt, birds = 0, obstacles = 0;
	bool object_detected = false;
	bool scatter = false;
	bool want_to_perch = parts[i].life > 0;

	// Stop perching
	if (parts[i].tmp2 < 0) {
		parts[i].tmp = 0;
		parts[i].life = 0;
		parts[i].tmp2 = 0;
	}
	// Perch countdown, freeze
	if (parts[i].tmp) {
		parts[i].vx = parts[i].vy = 0;
		parts[i].tmp2--;
		parts[i].life = 0;

		// Check if there is still a solid nearby to perch on
		for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
		if (BOUNDS_CHECK && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (r && sim->elements[TYP(r)].Properties & TYPE_SOLID) return 0;
		}

		// Stop perching, nothing to perch on
		parts[i].tmp2 = -1;
		return 0;
	}

	if (RNG::Ref().chance(1, 600))
		parts[i].life = 1;

	float flockvx = 0.0f, flockvy = 0.0f;
	float centerx = 0.0f, centery = 0.0f;
	float repulse_x = 0.0f, repulse_y = 0.0f;
	float repulse_object_x = 0.0f, repulse_object_y = 0.0f;

	// Avoid edges.
	if (x < EDGE_RANGE) repulse_object_x += EDGE_PUSH;
	else if (x > XRES - EDGE_RANGE) repulse_object_x -= EDGE_PUSH;
	if (y < EDGE_RANGE) repulse_object_y += EDGE_PUSH;
	else if (y > YRES - EDGE_RANGE) repulse_object_y -= EDGE_PUSH;
	if (repulse_object_x || repulse_object_y) {
		object_detected = true;
		obstacles = 1;
	}

	for (rx = -BIRD_SEARCH_RANGE; rx <= BIRD_SEARCH_RANGE; ++rx)
		for (ry = -BIRD_SEARCH_RANGE; ry <= BIRD_SEARCH_RANGE; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				if (rt == PT_BIRD) {
					// Tend towards flock direction
					flockvx += parts[ID(r)].vx;
					flockvy += parts[ID(r)].vy;

					centerx += parts[ID(r)].x;
					centery += parts[ID(r)].y;
					++birds;
					object_detected = true;

					// Tend away from nearby birds, the closer they are the stronger the repulsion
					if (abs(rx) <= BIRD_AVOID_RANGE && abs(ry) <= BIRD_AVOID_RANGE) {
						repulse_x -= rx;
						repulse_y -= ry;
					}
					continue;
				}

				// Tend towards food
				else if (rt == PT_ANT || rt == PT_SEED || rt == PT_FISH) {
					repulse_object_x += rx;
					repulse_object_y += ry;
					++obstacles;
					object_detected = true;
				}

				// Tend away from obstacles detected
				// Anything that's not a gas or too hot is an obstacle. Birds can't
				// see glass though
				else if (rt != PT_WEB && rt != PT_GLAS && rt != PT_TRBN && (parts[i].temp > AVOID_TEMP ||
						!(sim->elements[rt].Properties & TYPE_GAS) ||
						sim->IsWallBlocking(x + rx, y + ry, PT_BIRD))) {
					repulse_object_x -= rx;
					repulse_object_y -= ry;
					++obstacles;
					object_detected = true;

					// If birds see anything that's PROP_DEADLY or hot scatter
					if (sim->elements[rt].Properties & PROP_DEADLY || parts[i].temp > AVOID_TEMP ||
						rt == PT_LIGH || rt == PT_BOMB)
						scatter = true;

					// If want to perch attract instead to solids
					if (want_to_perch && sim->elements[rt].Properties & TYPE_SOLID && rt != PT_GLAS && rt != PT_WEB && rt != PT_TRBN) {
						repulse_object_x += 2 * rx;
						repulse_object_y += 2 * ry;
					}
				}

				// Collisions
				if (abs(rx) < 2 && abs(ry) < 2) {
					// Die upon colliding with glass
					if (rt == PT_GLAS && fabs(parts[i].vx + parts[i].vy) > 0.5f) {
						sim->part_change_type(i, parts[i].x, parts[i].y, PT_BLOD);
						return 0;
					}
					// Fried by electricity
					else if (rt == PT_SPRK || rt == PT_LIGH) {
						sim->part_change_type(i, parts[i].x, parts[i].y, PT_DUST);
						parts[i].temp += 100.0f;
						return 0;
					}
					// Stop perching if crowded
					else if (rt == PT_BIRD && RNG::Ref().chance(1, 3)) {
						parts[i].tmp2 = -1;
						return 0;
					}
					// Perch timer
					else if (want_to_perch && sim->elements[rt].Properties & TYPE_SOLID) {
						parts[i].tmp = 1;
						parts[i].tmp2 = RNG::Ref().between(50, 200);
						return 0;
					}
					// Eat food
					else if ((rt == PT_SEED || rt == PT_ANT || rt == PT_FISH) && RNG::Ref().chance(1, 100)) {
						sim->kill_part(ID(r));
						return 0;
					}
				}
			}

	if (object_detected) {
		if (obstacles) {
			// Avoid obstacles
			parts[i].vx += repulse_object_x / obstacles;
			parts[i].vy += repulse_object_y / obstacles;
		}

		else if (birds) {
			// Avoid being too close to other birds
			parts[i].vx += repulse_x / 3.0f / birds;
			parts[i].vy += repulse_y / 3.0f / birds;

			// Follow center of flock (If scatter is true do the opposite)
			if (!scatter) {
				parts[i].vx += (centerx / birds - parts[i].x) / 100.0f;
				parts[i].vy += (centery / birds - parts[i].y) / 100.0f;
			} else {
				parts[i].vx -= (centerx / birds - parts[i].x) / 100.0f;
				parts[i].vy -= (centery / birds - parts[i].y) / 100.0f;
			}

			// Match nearby velocities
			parts[i].vx += (flockvx / birds - parts[i].vx) / 8;
			parts[i].vy += (flockvy / birds - parts[i].vy) / 8;
		}
	}
	// Randomize dir if alone or not moving (and no obstacle to avoid)
	if (RNG::Ref().chance(1, 50) || (!object_detected && parts[i].vx + parts[i].vy < 0.2f)) {
		float angle = RNG::Ref().uniform01() * 2 * 3.141592f;
		float nvx = cos(angle), nvy = sin(angle);
		parts[i].vx += nvx * ACCELERATION, parts[i].vy += nvy * ACCELERATION;
	}

	// Tend towards spawn location
	// parts[i].vx += (parts[i].tmp3 - parts[i].x) / 40000.0f;
	// parts[i].vy += (parts[i].tmp4 - parts[i].y) / 40000.0f;

	// Limit velocity
	if (fabs(parts[i].vx) > MAX_VELOCITY) parts[i].vx = isign(parts[i].vx) * MAX_VELOCITY;
	if (fabs(parts[i].vy) > MAX_VELOCITY) parts[i].vy = isign(parts[i].vy) * MAX_VELOCITY;

	return 0;
}
