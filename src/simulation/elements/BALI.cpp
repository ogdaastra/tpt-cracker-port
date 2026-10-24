#include "simulation/ElementCommon.h"
#include <cmath>
#include <iostream>

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void Element_BALI_create(ELEMENT_CREATE_FUNC_ARGS);
void Element_EXFN_draw_glowy_pixel(Renderer *ren, int nx, int ny, int colr, int colg, int colb, int cola);

// SPOILERS
// Do not read this element code unless
// you want to be spoiled for Liu Cixin's Ball Lightning (novel)

// Ball lightning
// Invisible until it charges up with energy and glows a brilliant white (Depends on how much it was charged)
// It's energy output is electricity like LIGH/SPRK
// Emits a circle with same color
// Goes against air flow and after random amount of time, will explode
// Explosion will destroy only it's ctyle nearby replacing it with something that glows then turns into ash
namespace BALI_DATA {
	// Marker life value, r, g, b
	// For example, if life = 400, the color would be eased between
	// the color at life = 380 and life = 450
	const int wavelengths[9][4] = {
		{0, 0, 0, 0},
		{380, 128, 0, 128},
		{450, 0, 0, 255},
		{485, 0, 128, 128},
		{500, 0, 255, 0},
		{565, 128, 128, 0},
		{590, 255, 165, 0},
		{625, 255, 0, 0},
		{825, 255, 255, 255}
	};

	// Default ctypes to spawn with to destroy
	const int NUM_DEFAULT_CTYPES = 13;
	const int DEFAULT_CTYPES[NUM_DEFAULT_CTYPES] = {
		PT_PLNT, PT_IRON, PT_METL, PT_IRON, PT_TTAN, PT_GOLD, PT_GLAS, PT_QRTZ, PT_BRCK, PT_CRMC,
		PT_WOOD, PT_GOO, PT_SAND};

	void findBoundary(int life, int& colr, int& colg, int& colb, int &cola) {
		float lifediff;
		int rdiff, bdiff, gdiff;

		for (unsigned int i = 0; i < 8; ++i) {
			if (life > wavelengths[i][0] && life > wavelengths[i + 1][0]) continue; // Stop at first life we're between 2 boundaries

			// Linear easing, we take the range between the 2 colors
			// and do a proportional averaging, ie, if:
			// 		life = 100 and r1 = 255
			// 		life = 200 and r2 = 100
			// and life is currently 125 (25% from 255 to 0)
			// we compute new life value by taking (life - min) / range * (rdiff) + r2
			// (125 - 100) / (200 - 100) * (100 - 255) + 255
			// = 0.25 * -155 + 255 = 216.75
			lifediff = wavelengths[i + 1][0] - wavelengths[i][0];
			rdiff = wavelengths[i + 1][1] - wavelengths[i][1];
			gdiff = wavelengths[i + 1][2] - wavelengths[i][2];
			bdiff = wavelengths[i + 1][3] - wavelengths[i][3];

			colr = (life - wavelengths[i][0]) / lifediff * rdiff + wavelengths[i][1];
			colg = (life - wavelengths[i][0]) / lifediff * gdiff + wavelengths[i][2];
			colb = (life - wavelengths[i][0]) / lifediff * bdiff + wavelengths[i][3];
			cola = i == 0 ? (life - wavelengths[i][0]) / lifediff * 255 : 255;
			return;
		}

		// It's white now
		colr = colg = colb = 255;
	}
}

void Element::Element_BALI() {
	Identifier = "DEFAULT_PT_BALI";
	Name = "BALI";
	Colour = PIXPACK(0xDFEFFF);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;

	Advection = -0.7f;
	AirDrag = -0.01f * CFDS;
	AirLoss = 0.96f;
	Loss = 0.5f;
	Collision = 0.0f;
	Gravity = 0.0f;
	Diffusion = 0.50f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 1;
	HeatConduct = 251;
	Description = "Ball Lightning. Actually a macroelectron, releases energy onto its ctype.";
	Properties = TYPE_GAS;

	Update = &update;
	Graphics = &graphics;
	Create = &Element_BALI_create;
}

void Element_BALI_create(ELEMENT_CREATE_FUNC_ARGS) {
	sim->parts[i].tmp = RNG::Ref().between(600, 4999);
	sim->parts[i].life = RNG::Ref().between(380, 625);

	// Random ctype to destroy
	sim->parts[i].ctype = BALI_DATA::DEFAULT_CTYPES[RNG::Ref().between(0, BALI_DATA::NUM_DEFAULT_CTYPES)];
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * life = energy level
	 * ctype = target element to act upon
	 * tmp = Time before explosion
	 * tmp2 = If not 0, will simply stay still and glow then disappear
	 */

	// Stay still if tmp2
	if (parts[i].tmp2) {
		parts[i].vx = parts[i].vy = 0;
	}

	int r, rx, ry;
	float randInt = RNG::Ref().uniform01();

	parts[i].tmp--;
	if (parts[i].tmp <= 0 || parts[i].life <= 0) {
		// Explode if enough power
		if (parts[i].life > 100) {
			// Create smoke and pressure
			sim->pv[y / CELL][x / CELL] += 60.0f;
			sim->create_part(-1, x - 1, y, PT_SMKE);
			sim->create_part(-1, x + 1, y, PT_SMKE);
			sim->create_part(-1, x, y - 1, PT_SMKE);
			sim->create_part(-1, x, y + 1, PT_SMKE);

			// Radius is equal to round(life / 20)
			// Deletion radius caps out at 60 px
			int radius = round(parts[i].life / 20.0f);
			radius = radius > 60 ? 60 : radius;
			int rsquared = radius * radius;

			for (rx = -radius; rx <= radius; rx++)
			for (ry = -radius; ry <= radius; ry++)
				if (BOUNDS_CHECK) {
					r = pmap[y + ry][x + rx];
					if (!r) r = sim->photons[y + ry][x + rx];
					if (!r) continue;

					if (TYP(r) == parts[i].ctype && (rx * rx + ry * ry) <= rsquared) {
						parts[ID(r)].life = parts[ID(r)].tmp = 15;
						parts[ID(r)].tmp2 = 1;
						parts[ID(r)].ctype = TYP(r);
						sim->part_change_type(ID(r), x + rx, y + ry, PT_BALI);
					}
				}
		}

		if (parts[i].tmp2 && RNG::Ref().chance(1, 50)) // Turn to ash
			sim->part_change_type(i, parts[i].x, parts[i].y, PT_BCOL);
		else  // Actual BALI disappears
			sim->kill_part(i);
		return 0;
	}

	if (parts[i].tmp2 == 0 && parts[i].life >= 0) {
		for (rx = -3; rx <= 3; rx++)
			for (ry = -3; ry <= 3; ry++)
				if (BOUNDS_CHECK && (rx || ry)) {
					r = pmap[y + ry][x + rx];
					if (!r) r = sim->photons[y + ry][x + rx];
					if (!r) continue;

					// Electric elements increase BALI's energy level
					// SPRK was removed since it can SPRK elements and
					// supercharge itself
					if (TYP(r) == PT_ELEC)
						parts[i].life++;
					else if (TYP(r) == PT_LIGH || TYP(r) == PT_THDR)
						parts[i].life += RNG::Ref().between(380, 600);

					// BALI absorbs other BALI only if this has more energy
					if (TYP(r) == PT_BALI && parts[i].life >= parts[ID(r)].life) {
						parts[i].life += parts[ID(r)].life;
						sim->kill_part(ID(r));
						return 0;
					}

					// Randomly spark elements if there are
					if (randInt < 0.05f) {
						if (sim->elements[TYP(r)].Properties & PROP_CONDUCTS) {
							parts[i].life -= 10;
							parts[ID(r)].life = 4;
							parts[ID(r)].ctype = TYP(r);
							sim->part_change_type(ID(r), x + rx, y + ry, PT_SPRK);
						}
					}
				}
	}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Stand still and glow, we can just glow this 1 px
	if (cpart->tmp2) {
		*pixel_mode |= FIRE_ADD;
		*colr = *colg = *colb = 255;
		*firer = *fireg = *fireb = 255;
		*firea = 100;
		return 0;
	}

	*colr = *colg = *colb = 0;
	*cola = 255; // Black

	// Set current color
	BALI_DATA::findBoundary(cpart->life, *colr, *colg, *colb, *cola);
	int rx, ry, r;

	// Radius of inner circle, pulsates between 3 and 5
	int FOUR_RADIUS = 4 + sin(ren->sim->timer / 100.0f);
	FOUR_RADIUS *= FOUR_RADIUS;

	for (rx = -6; rx <= 6; ++rx)
	for (ry = -6; ry <= 6; ++ry)
		if (BOUNDS_CHECK) {
			r = rx * rx + ry * ry;
			r *= (1 + 1.5f * RNG::Ref().uniform01());
			if (r <= FOUR_RADIUS) // Radius 4, pulsates. White glow inside
				Element_EXFN_draw_glowy_pixel(ren, cpart->x + rx, cpart->y + ry, 255, 255, 255, *cola);
			else if (r <= 36) // Radius 6, outer glow (colored)
				Element_EXFN_draw_glowy_pixel(ren, cpart->x + rx, cpart->y + ry, *colr, *colg, *colb, *cola);
		}

	// Actual middle particle is invisible, make it white to blend in if its white
	if (*cola != 0) *colr = *colg = *colb = 255;
	return 0;
}
