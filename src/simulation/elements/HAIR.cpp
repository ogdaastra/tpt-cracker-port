#include "simulation/ElementCommon.h"
#include <iostream>

static int update(UPDATE_FUNC_ARGS);

#define HEAD_RADIUS 12
#define PLAYER_LINK_CHECK(player, newtmp) if (player.spwn) { \
			xdiff = parts[player.stkmID].x - parts[i].x, \
			ydiff = parts[player.stkmID].y - parts[i].y; \
			if (sqrtf(xdiff * xdiff + ydiff * ydiff) <= HEAD_RADIUS) { \
				parts[i].tmp = newtmp; \
				goto attach_hair_end; \
			} \
		}

void Element::Element_HAIR() {
	Identifier = "DEFAULT_PT_HAIR";
	Name = "HAIR";
	Colour = PIXPACK(0x33302C);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_CRACKER2 in TPT Ultimata
	Enabled = 1;

	Advection = 0.7f;
	AirDrag = 0.02f * CFDS;
	AirLoss = 0.96f;
	Loss = 0.80f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 10;
	Explosive = 0;
	Meltable = 0;
	Hardness = 30;
	Weight = 85;
	HeatConduct = 70;

	Description = "Hair. Sticks to STKM heads, gets dirty. Wash with SHPO.";

	Properties = TYPE_PART | PROP_NEUTPENETRATE | PROP_NOCTYPEDRAW;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 232.778f + 273.15f;
	HighTemperatureTransition = PT_FIRE;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * tmp  - attached to STKM currently (-1 = GELed or frozen, 0 = not yet, 1 = STKM, 2 = STKM2, higher = FIGH)
	 * tmp2 - 1 = "base" strand, 2 = attached to a base strand
	 * life - ID of strand to attach to, if secondary
	 * tmp3/4 - Relative position to id
	 */

	// Invalid if >= max fighters
	if (parts[i].tmp >= 2 + MAX_FIGHTERS)
		parts[i].tmp = parts[i].tmp2 = 0;

	// Freeze if cold
	if (parts[i].temp < 0 && parts[i].tmp2 == 0)
		parts[i].tmp = -1;
	
	// Disconnect hair if STKM dies
	if (parts[i].tmp == 1 && !sim->player.spwn)
		parts[i].tmp = parts[i].tmp2 = 0;
	else if (parts[i].tmp == 2 && !sim->player2.spwn)
		parts[i].tmp = parts[i].tmp2 = 0;
	else if (parts[i].tmp > 2 && !sim->fighters[parts[i].tmp - 2].spwn)
		parts[i].tmp = parts[i].tmp2 = 0;
	
	// Attach hair if not already attached
	float xdiff = 0.0f, ydiff = 0.0f;
	if (parts[i].tmp == 0) {
		PLAYER_LINK_CHECK(sim->player, 1)
		PLAYER_LINK_CHECK(sim->player2, 2)
		
		for (unsigned int j = 0; j < MAX_FIGHTERS; ++j) {
			PLAYER_LINK_CHECK(sim->fighters[j], j + 2)
		}
	}
	// GOTO For attaching to STKM
	if (false) {
		attach_hair_end:
		parts[i].tmp3 = (int)(xdiff + 0.5f);
		parts[i].tmp4 = (int)(ydiff + 0.5f);
		parts[i].tmp2 = 1;
	}

	// Attach hair to other attached hair
	// Get hair dirty
	int rx, ry, r, rt;
	parts[i].flags = 0; // Track how many empty spaces
	for (rx = -2; rx <= 2; ++rx)
		for (ry = -2; ry <= 2; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				// Attach to other hair
				if (rt == PT_HAIR && parts[i].tmp2 == 0 && parts[ID(r)].tmp == 1) {
					parts[i].tmp2 = 2;
					parts[i].tmp3 = rx;
					parts[i].tmp4 = ry;
					parts[i].life = ID(r);
				}
				// Gel
				else if (rt == PT_GEL && parts[i].tmp2 == 0) {
					parts[i].tmp = -1;
				}
				// "Stain" with dcolour of other elements
				else if (rt != PT_HAIR && sim->elements[rt].Properties & TYPE_PART && RNG::Ref().between(1, 100)) {
					parts[i].dcolour = 0xFF000000 + sim->elements[rt].Colour;
				}
			}

	// Move hair to target
	auto stkm = sim->player;
	if (parts[i].tmp == 2)
		stkm = sim->player2;
	else if (parts[i].tmp > 2)
		stkm = sim->fighters[parts[i].tmp - 2];

	// Check if hair is actually attached still
	// (No extreme movements)
	if (parts[i].tmp2 > 0 && sim->timer % 10 == 0) {
		// Base strand: check STKM
		if (parts[i].tmp2 == 1) {
			int dx = parts[stkm.stkmID].x - x, dy = parts[stkm.stkmID].y - y;
			int dis = sqrt(dx * dx + dy * dy);

			if (dis > 2 * HEAD_RADIUS) {
				parts[i].tmp = 0;
				parts[i].tmp2 = 0;
			}
		}
	}

	// Attached to STKM
	if (parts[i].tmp > 0) {
		parts[i].x = parts[stkm.stkmID].x - parts[i].tmp3;
		parts[i].y = parts[stkm.stkmID].y - parts[i].tmp4;
	}
	// Attached to other hair
	else if (parts[i].tmp2 == 2) {
		// Base strand no longer valid
		if (parts[parts[i].life].type != PT_HAIR || parts[parts[i].life].tmp2 != 1) {
			parts[i].tmp = 0;
			parts[i].tmp2 = 0;
		}
		else {
			parts[i].x = parts[parts[i].life].x - parts[i].tmp3;
			parts[i].y = parts[parts[i].life].y - parts[i].tmp4;
		}
	}

	// Stop moving if hair is attached or GELed
	if (parts[i].tmp2 != 0 || parts[i].tmp == -1)
		parts[i].vx = parts[i].vy = 0;

	return 0;
}
