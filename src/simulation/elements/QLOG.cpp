#include "simulation/ElementCommon.h"
#include "simulation/quantum/def.h"
#include "simulation/quantum/qmath.h"
#include "simulation/quantum/linalg.h"
#include "simulation/quantum/quantum.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

#include <iostream>

void Element::Element_QLOG() {
	Identifier = "DEFAULT_PT_QLOG";
	Name = "QLOG";
	Colour = PIXPACK(0xCF3A3A);
	MenuVisible = 1;
	MenuSection = SC_ELEC;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.90f;
	Loss = 0.00f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 100;

	HeatConduct = 0;
	Description = "Quantum Logic Gate. Fires a beam that alters ION like ARAY. Ctype determines logic gate type.";

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	DefaultProperties.life = 0;

	Update = &update;
	Graphics = &graphics;
}

static int update_scan(Particle *parts, GridView<int> pmap, int i, int tx, int ty, bool multi_check) {
	// Func when sparking, sets life and determines a gate's other inputs
	int id = ID(pmap[ty][tx]);

	if (parts[id].ctype == parts[i].ctype) {
		if (multi_check && parts[id].tmp == parts[i].tmp) {
			if (parts[id].tmp2 == 1 && parts[i].tmp3 < 0)
				parts[i].tmp3 = id;
			else if (parts[id].tmp2 == 2 && parts[i].tmp4 < 0)
				parts[i].tmp4 = id;
		}
	}
	return 0;
}

static int find_ion(Particle *parts, GridView<int> photons, GridView<int> pmap, int i) {
	int dx, dy; dx = dy = 0;
	int tx = parts[i].x;
	int ty = parts[i].y;
	if (parts[i].tmp == 0 || parts[i].tmp == 2)
		dx = parts[i].tmp == 0 ? -1 : 1;
	else dy = parts[i].tmp == 1 ? -1 : 1;

	// Keep searching until we hit a non-QLOG element or ION
	while (ty >= 0 && ty < YRES && tx >= 0 && tx < XRES &&
			(!pmap[ty][tx] || TYP(pmap[ty][tx]) == PT_QLOG) &&
			(!photons[ty][tx] || TYP(photons[ty][tx]) != PT_ION)) {
		tx += dx;
		ty += dy;
	}

	// Couldn't find an ION
	if (!photons[ty][tx] || TYP(photons[ty][tx]) != PT_ION)
		return -1;
	return ID(photons[ty][tx]);
}

static int update(UPDATE_FUNC_ARGS) {
	/* 
	 * Properties
     * - flags: For non-first input, stores location of first input
	 * - tmp: beam direction
	 * - tmp2: input number (for multi gate inputs, 0 indexed)
	 * - ctype: logic gate type
	 * 
	 * For multi-input gates (up to 3 inputs)
	 * - tmp3, tmp4: ID(r) of the other 2 gate inputs (Assumes self is input 0)
	 */
	int rx, ry, r;

    /* When firing the beam searches for any ION in a straight line to modify. */
	if (!QGATE_NAME::does_nothing(parts[i].ctype) && parts[i].life == QGATE_NAME::COOLDOWN) {
		parts[i].life--;
		parts[i].flags = -1;

		// Only multi gate input 0 actually does anything
		if (parts[i].tmp2 > 0)
			return 0;

		// If current gate input > 1 find the other gates
		int tx = parts[i].x, ty = parts[i].y;

		// Reset before searching
		parts[i].tmp3 = -1;
		parts[i].tmp4 = -1;

		if (parts[i].tmp == 0 || parts[i].tmp == 2) { // Search vertical
			ty++;
			while (ty >= 0 && ty < YRES && parts[ID(pmap[ty][tx])].type == PT_QLOG) {
				update_scan(parts, pmap, i, tx, ty, true);
				ty++;
			}
			ty = parts[i].y - 1;
			while (ty >= 0 && ty < YRES && parts[ID(pmap[ty][tx])].type == PT_QLOG) {
				update_scan(parts, pmap, i, tx, ty, true);
				ty--;
			}
		}
		else { // Search horizontal
			tx++;
			while (tx >= 0 && tx < XRES && parts[ID(pmap[ty][tx])].type == PT_QLOG) {
				update_scan(parts, pmap, i, tx, ty, true);
				tx++;
			}
			tx = parts[i].x - 1;
			while (tx >= 0 && tx < XRES && parts[ID(pmap[ty][tx])].type == PT_QLOG) {
				update_scan(parts, pmap, i, tx, ty, true);
				tx--;
			}
		}

		// Find the ion
		std::vector<int> IONS;
		IONS.push_back(find_ion(parts, sim->photons, pmap, i));

		// -- Actual gate logic begins here -- //
		// ----------------------------------- //
		matrix gate = QGATE::get_matrix_by_id(parts[i].ctype);
		if (gate.size() == 0 && 
				parts[i].ctype != QGATE_NAME::RESET &&
				parts[i].ctype != QGATE_NAME::DETECT) // Invalid gate
			goto end_logic;

		// Additional logic for multi gates to check all inputs are accounted for
		// We check gates from 2 inputs to 3 inputs
		// > Self = Input 0
		// > tmp3 = Input 1
		// > tmp4 = Input 2
		for (int shift = 2; shift <= 3; ++shift) {
			// (2^n by 2^n matrix for 2^n particle state, where n is number of inputs)
			if ((int)gate.size() >= (1 << shift)) {
				// Failed to find a 2ND input (All checks must be present as element might have moved)
				// or been deleted since last frame
				auto& tmp3 = parts[i].tmp3;
				auto& tmp4 = parts[i].tmp4;
				int to4 = shift - 2;
				if ((to4 ? tmp4 : tmp3) == -1 || parts[to4 ? tmp4 : tmp3].type != PT_QLOG ||
						parts[i].ctype != parts[to4 ? tmp4 : tmp3].ctype ||
						parts[i].tmp != parts[to4 ? tmp4 : tmp3].tmp ||
						parts[to4 ? tmp4 : tmp3].tmp2 != to4 + 1)
					goto end_logic;
				IONS.push_back(find_ion(parts, sim->photons, pmap, (to4 ? tmp4 : tmp3)));
			}
		}

		// Sanity check for ION
		for (unsigned int l = 0; l < IONS.size(); ++l) {
			if (IONS[l] < 0)
				goto end_logic; // Invalid ION found

			// IONs only gain quantum states when worked on by a
			// QLOG (so just normal stray IONs don't decohere every 0.1s wasting
			// computational cycles. Whether an ION has a quantum state is determined
			// by its flags property)
			if (parts[IONS[l]].flags == 0) {
				QUANTUM::create_particle_state(parts, IONS[l]);
				parts[IONS[l]].flags = 1;
			}
		}

		switch(parts[i].ctype) {
			// Reset ION state gate
			case QGATE_NAME::RESET: {
				if (IONS.size() == 0) break;
				QUANTUM::create_particle_state(parts, IONS[0]);
				parts[IONS[0]].flags = 1;
				break;
			}
			// Detect gate is special
			case QGATE_NAME::DETECT: {
				if (IONS.size() == 0) break;
				int spin = QUANTUM::measure(IONS[0], parts[IONS[0]].tmp2, parts[i].tmp2);
				if (spin == 0)
					goto end_logic; // Don't spark if spindown
				
				for (rx = -1; rx <= 1; rx++)
				for (ry = -1; ry <= 1; ry++)
					if (BOUNDS_CHECK && (rx || ry) && (rx == 0 || ry == 0)) {
						r = pmap[y + ry][x + rx];
						if (!r) continue;

						// Activate WIFI
						if (TYP(r) == PT_WIFI) {
							// same channel layout as WIFI.cpp: [Faraday region (+100 with tmp2)][temperature channel]
							int faraday = sim->faraday_map[(y + ry) / CELL][(x + rx) / CELL] + (parts[ID(r)].tmp2 ? MAX_FARADAY_DIVISIONS : 0);
							sim->wireless[faraday][parts[ID(r)].tmp] |= 1;
							sim->ISWIRE = 2;
							continue;
						}

						// SPRK nearby metal
						if (!(sim->elements[TYP(r)].Properties & PROP_CONDUCTS))
							continue;

						parts[ID(r)].life = 4;
						parts[ID(r)].ctype = TYP(r);
						sim->part_change_type(ID(r), x + rx, y + ry, PT_SPRK);
					}
				break;
			}
			default: {
				QUANTUM::apply_gate(gate, parts, IONS);
				break;
			}
		}

		end_logic:

		// Reset tmp3/4
		parts[i].tmp3 = -1;
		parts[i].tmp4 = -1;

		return 0; // Stop now since we already fired and updated
	} else if (parts[i].life > 0) {
		parts[i].life--;
		parts[i].flags = -1;
		return 0;
	}


	/* Beam directions: tmp = 0: left, 1 = up, 2 = right, 3 = down */
	int dir = parts[i].tmp;

	for (rx = -1; rx <= 1; rx++)
		for (ry = -1; ry <= 1; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				
				/* If it detects a spark, fire in the direction of tmp and activates any QLOG
				 * in an orthagonal direction to the beam. SPRK cannot be detected diagonally. */
				if (!(rx && ry) && parts[ID(r)].type == PT_SPRK) {
					int tx = parts[i].x;
					int ty = parts[i].y;

					if (dir == 0 || dir == 2) { // Search vertical
						ty++;
						while (ty >= 0 && ty < YRES && parts[ID(pmap[ty][tx])].type == PT_QLOG) {
							parts[ID(pmap[ty][tx])].life = QGATE_NAME::COOLDOWN;
							ty++;
						}
						ty = parts[i].y - 1;
						while (ty >= 0 && ty < YRES && parts[ID(pmap[ty][tx])].type == PT_QLOG) {
							parts[ID(pmap[ty][tx])].life = QGATE_NAME::COOLDOWN;
							ty--;
						}
					}
					else { // Search horizontal
						tx++;
						while (tx >= 0 && tx < XRES && parts[ID(pmap[ty][tx])].type == PT_QLOG) {
							parts[ID(pmap[ty][tx])].life = QGATE_NAME::COOLDOWN;
							tx++;
						}
						tx = parts[i].x - 1;
						while (tx >= 0 && tx < XRES && parts[ID(pmap[ty][tx])].type == PT_QLOG) {
							parts[ID(pmap[ty][tx])].life = QGATE_NAME::COOLDOWN;
							tx--;
						}
					}
					parts[i].life = QGATE_NAME::COOLDOWN; // Set self life
				}
			}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Vary color based on ctype
	if (cpart->ctype % 2 == 0) *colb += 30 * cpart->ctype;
	else *colg += 30 * cpart->ctype;

	// Draw beam if it should
	if (cpart->life <= 0) return 0;
	if (cpart->life == QGATE_NAME::COOLDOWN) {
		int ION = find_ion(ren->sim->parts, ren->sim->photons, ren->sim->pmap, ID(ren->sim->pmap[(int)cpart->y][(int)cpart->x]));
		if (ION >= 0) {
			ren->draw_line((int)cpart->x, (int)cpart->y, ren->sim->parts[ION].x, ren->sim->parts[ION].y,
				*colr, *colg, *colb, 100);
		}
	}

	// Brighten when triggered
	*colr = (int)(*colr * 1.5);
	*colg = (int)(*colg * 1.5);
	*colb = (int)(*colb * 1.5);

	return 0;
}
