#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

namespace EXFN_DATA {
	const int LEFT = 0;
	const int TOP = 1;
	const int RIGHT = 2;
	const int BOTTOM = 3;
	const float TWO_PI = 3.141592 * 2;

	bool is_up(int dir) {
		return dir == TOP || dir == BOTTOM;
	}

	bool is_part_of_wave(int flags, int period, int delta, int direction_multiplier, int timer, int offset,
			int tmp3) {
		float result = flags / 2;
		result = result + result * sin(TWO_PI / period *
			(offset + delta + direction_multiplier * timer));
		result = int(result) + 1;
		return result == tmp3 || result == flags - tmp3;
	}

	void set_directions(int &spawndx, int &spawndy, int &initx, int &inity, int &dir, int x, int y, int tmp) {
		dir = tmp;
		if (is_up(dir)) {
			spawndx = 0;
			spawndy = dir == TOP ? -1 : 1;
		}
		else {
			spawndy = 0;
			spawndx = dir == LEFT ? -1 : 1;
		}

		initx = x + spawndx;
		inity = y + spawndy;
	}
}

void Element::Element_EXFN() {
	Identifier = "DEFAULT_PT_EXFN";
	Name = "EXFN";
	Colour = PIXPACK(0x645E8A);
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
	HotAir = 0.000f	* CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 0;

	Weight = 100;

	DefaultProperties.temp = 273.15;
	HeatConduct = 0;
	Description = "Excursion Funnel. Creates a stasis beam (direction depends on tmp). Toggle reverse with GOLD / TTAN";

	Properties = TYPE_SOLID;

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

void Element_EXFN_draw_glowy_pixel(Renderer *ren, int nx, int ny, int colr, int colg, int colb, int cola) {
		// Non-glowy render mode
		if (ren->colour_mode & COLOUR_HEAT || !(ren->render_mode & PMODE_GLOW)) {
			ren->addpixel(nx, ny, colr, colg, colb, cola);
			return;
		}

		int cola1 = (5 * cola) / 255;
		ren->addpixel(nx, ny, colr, colg, colb, (192*cola)/255);
		ren->addpixel(nx+1, ny, colr, colg, colb, (96*cola)/255);
		ren->addpixel(nx-1, ny, colr, colg, colb, (96*cola)/255);
		ren->addpixel(nx, ny+1, colr, colg, colb, (96*cola)/255);
		ren->addpixel(nx, ny-1, colr, colg, colb, (96*cola)/255);

		for (int x = 1; x < 6; x++) {
			ren->addpixel(nx, ny-x, colr, colg, colb, cola1);
			ren->addpixel(nx, ny+x, colr, colg, colb, cola1);
			ren->addpixel(nx-x, ny, colr, colg, colb, cola1);
			ren->addpixel(nx+x, ny, colr, colg, colb, cola1);
			for (int y = 1; y < 6; y++) {
				if(x + y > 7)
					continue;
				ren->addpixel(nx+x, ny-y, colr, colg, colb, cola1);
				ren->addpixel(nx-x, ny+y, colr, colg, colb, cola1);
				ren->addpixel(nx+x, ny+y, colr, colg, colb, cola1);
				ren->addpixel(nx-x, ny-y, colr, colg, colb, cola1);
			}
		}
	}



static int update(UPDATE_FUNC_ARGS) {
	int r, rx, ry, id;
	int spawndx, spawndy, initx, inity, dir;
	EXFN_DATA::set_directions(spawndx, spawndy, initx, inity, dir, parts[i].x, parts[i].y, parts[i].tmp);

	if (parts[i].temp>=256.0+273.15)
		parts[i].temp=256.0+273.15;
	if (parts[i].temp<= -256.0+273.15)
		parts[i].temp = -256.0+273.15;

	/* EXFN properties with not EXFN:
	 * - SPRK (.ctype GOLD): toggle self tmp2 = 0
	 * - SPRK (.ctype TTAN): toggle self tmp2 = 10
	 * - SPRK (.ctype PSCN): toggle life = 10
	 * - SPRK (.ctype NSCN): toggle life = 0
	 */
	for (rx = -1; rx <= 1; rx++)
		for (ry = -1; ry <= 1; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				id = ID(r);

				if (TYP(r) == PT_SPRK) {
					if (parts[id].ctype == PT_GOLD || parts[id].ctype == PT_TTAN) {
						PropertyValue value;
						value.Integer = parts[id].ctype == PT_GOLD ? 10 : 0;
						sim->flood_prop(parts[i].x, parts[i].y, offsetof(Particle, tmp2), value, StructProperty::Integer);
					}
					else if (parts[id].ctype == PT_PSCN || parts[id].ctype == PT_NSCN) {
						PropertyValue value;
						value.Integer = parts[id].ctype == PT_PSCN ? 10 : 0;
						sim->flood_prop(parts[i].x, parts[i].y, offsetof(Particle, life), value, StructProperty::Integer);
					}
				}
			}

	/* Flags store total formation height / width
	 * If particle is rightmost or bottommost this is the tmp3 */
	if (parts[i].flags == 0) {
		int tx = parts[i].x + (EXFN_DATA::is_up(dir) ? 1 : 0); // Intentionally swapped dy and dx
		int ty = parts[i].y + (EXFN_DATA::is_up(dir) ? 0 : 1);
		if (ty >= 0 && ty >= 0 && tx < XRES && ty < YRES && TYP(pmap[ty][tx]) != PT_EXFN) {
			PropertyValue value;
			value.Integer = parts[i].tmp3;
			sim->flood_prop(parts[i].x, parts[i].y, offsetof(Particle, flags), value, StructProperty::Integer);
		}
	}

	/* EXFN properties with other EXFN
	 * - life: powered state, 1 = on, 0 = off. If life > 10 will slowly decrease to 0
	 *         and "spread" its powered state to others. Likewise, if life = 1 and a nearby
	 * 		   particle has life = 0, it will "spread" to life = 0. Toggles with NSCN and PSCN
	 * - tmp: 0 = left, 1 = top, 2 = right, 3 = bottom
	 * - tmp2: reversed, 1 = reversed, 0 = not. Spreads like life, toggles with GOLD
	 * - tmp3: depending on direction, if
	 * 		particle is leftmost or topmost, it will have a tmp3 of 1
	 * 		any other particle = particle to left or top + 1
	 */
	// Slowly decrement life and tmp2 to 1 if > 1
	if (parts[i].life > 1)
		parts[i].life--;
	if (parts[i].tmp2 > 1)
		parts[i].tmp2--;

	// Count particles and set appropriate tmp3
	// NOTE: Depends on particles being updated top->bottom, left->right
	int initial_tmp3 = parts[i].tmp3;
	int delta = 1;
	while (delta >= 0) {
		if (EXFN_DATA::TOP == dir || EXFN_DATA::BOTTOM == dir) // Vertical dir, go across
			r = pmap[y][x - delta];
		else r = pmap[y - delta][x]; // Horz beam, go vertical

		// Rightmost or bottom-most
		if (!r) break;

		if (parts[ID(r)].tmp3 != 0) {
			parts[i].tmp3 = parts[ID(r)].tmp3 + delta;
			break;
		}
		delta--;
	}

	// Leftmost or topmost
	if (parts[i].tmp3 == 0)
		parts[i].tmp3 = 1;

	// Initial tmp3 differs, reset flags!
	if (parts[i].tmp3 != initial_tmp3)
		parts[i].flags = 0;

	// Check for other EXFN
	for (rx = -2; rx <= 2; rx++)
		for (ry = -2; ry <= 2; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				id = ID(r);

				if (TYP(r) == PT_EXFN) {
					if (parts[i].life == 1 && parts[id].life == 0)
						parts[i].life = 0;
					else if (parts[i].life == 0 && parts[id].life > 1)
						parts[i].life = 10;
					if (parts[i].tmp2 == 1 && parts[id].tmp2 == 0)
						parts[i].tmp2 = 0;
					else if (parts[i].tmp2 == 0 && parts[id].tmp2 > 1)
						parts[i].tmp2 = 10;
				}
			}

	return 0;
}

int Element_EXFN_draw_beam(GRAPHICS_FUNC_ARGS) {
	// Color for the beam
	int r, g, b;
	if (cpart->tmp2 == 0) {
		r = 84; g = 127; b = 255;
	} else {
		r = 255; g = 152; b = 84;
	}

	// Draw the output funnels
	int period = cpart->flags * 4; // Period = 4 * height
	int spawndx, spawndy, initx, inity, dir, r2;
	int rv = cpart->tmp2 == 0 ? 1 : -1;
	float nudge_speed;
	EXFN_DATA::set_directions(spawndx, spawndy, initx, inity, dir, cpart->x, cpart->y, cpart->tmp);

	if (cpart->life > 0 && period > 0) {
		while (initx >= 0 && inity >= 0 && initx < XRES && inity < YRES) {
			// Don't overwrite any solids
			// The exception is glass (ID 45)
			// and MVSD (ID 196)
			r2 = ren->sim->pmap[inity][initx];
			if (r2 && (ren->sim->elements[TYP(r2)].Properties & TYPE_SOLID) && TYP(r2) != 45 &&  TYP(r2) != 196) break;

			// Set the stasis field (/4 since it resets every 4 frames)
			ren->sim->stasis->vx[(inity) / STASIS_CELL][(initx) / STASIS_CELL] += rv * (float)spawndx / 4;
			ren->sim->stasis->vy[(inity) / STASIS_CELL][(initx) / STASIS_CELL] += rv * (float)spawndy / 4;
			ren->sim->stasis->strength[(inity) / STASIS_CELL][(initx) / STASIS_CELL] = (cpart->temp - (273.15f - 256.0f)) * 1.6f / 512;

			// Beams should push gently towards center
			if (cpart->tmp3 < cpart->flags / 2)
				nudge_speed = 0.5f;
			else if(cpart->tmp3 == cpart->flags / 2)
				nudge_speed = 0.0f;
			else
				nudge_speed = -0.5f;

			if (EXFN_DATA::is_up(dir)) {
				ren->sim->stasis->vx[(inity) / STASIS_CELL][(initx) / STASIS_CELL] += nudge_speed / 4;
			} else {
				ren->sim->stasis->vy[(inity) / STASIS_CELL][(initx) / STASIS_CELL] += nudge_speed / 4;
			}

			/* Function to graph: tmp3 = (flags / 2) + (flags / 2)sin(a(initx - parts[i].x - time))
			 * where a = 2pi / period, flags is 2 * amplitude, tmp3 is y value from 0 to flags
			 * 
			 * The above example is for a wave going left. If the wave is going right then
			 * direction_multiplier is *= -1.
			 * 
			 * If the wave is going up or down then initx - cpart->x is replaced with y
			 * Delta reverse is to map it on the "wrong" axis to make a double back and forth
			 * scanning line
			 * 
			 * Other fun things: add -(period * 0.5) after the -time in the sin() to skew the graph
			 */
			int direction_multiplier = cpart->tmp2 == 0 ? 1 : -1; // Direction wave animates
			int delta = initx - cpart->x;
			int delta_reverse = inity - cpart->y;

			if (dir == EXFN_DATA::RIGHT || dir == EXFN_DATA::BOTTOM)
				direction_multiplier *= -1;

			if (EXFN_DATA::is_up(dir)) {
				delta = inity - cpart->y;
				delta_reverse = initx - cpart->x;
			}

			if (EXFN_DATA::is_part_of_wave(cpart->flags, period, delta, direction_multiplier, ren->sim->timer, 0, cpart->tmp3))
				Element_EXFN_draw_glowy_pixel(ren, initx, inity, r, g, b, 255);

			// The portal 2 graphics have another 3rd wave offset
			// about 1/3 of a period with a different color
			if (EXFN_DATA::is_part_of_wave(cpart->flags, period, delta, direction_multiplier, ren->sim->timer, period / 3, cpart->tmp3))
				Element_EXFN_draw_glowy_pixel(ren, initx, inity, r, g, b, 80);

			// Scanning lines
			if (EXFN_DATA::is_part_of_wave(cpart->flags, period, delta_reverse, direction_multiplier, ren->sim->timer, 0, cpart->tmp3))
				Element_EXFN_draw_glowy_pixel(ren, initx, inity, r, g, b, 80);

			initx += spawndx;
			inity += spawndy;
		}
	}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->life > 0) { // Brighten color if powered
		*colb *= 2;
		*colg *= 2;
		*colr *= 2;
	}

	Element_EXFN_draw_beam(ren, cpart, nx, ny, pixel_mode, cola, colr, colg, colb, firea, firer, fireg, fireb);
	return 0;
}
