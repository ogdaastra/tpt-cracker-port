#include "simulation/ElementCommon.h"
#include "music/audio.h"
#include "music/music.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_NOTE() {
	Identifier = "DEFAULT_PT_NOTE";
	Name = "NOTE";
	Colour = PIXPACK(0xDDDDDD);
	MenuVisible = 1;
	MenuSection = SC_ELEC;
	Enabled = 1;

	Advection = 0.00f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.00f;
	Loss = 0.00f;
	Collision = 0.00f;
	Gravity = 0.0f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 0;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 100;

	Weight = 100;
	HeatConduct = 0;
	Description = "Plays a note when sparked, life = length, tmp corresponds to piano key, tmp2 = instrument (0 to 4). (Earrape warning)";

	DefaultProperties.tmp = 49; // A
	DefaultProperties.life = 30; // Default length
	Properties = TYPE_SOLID;
	
	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	int rx, ry, rt, r;
	float freq = NOTE::get_frequency_from_key(parts[i].tmp);
	if (freq < 0) freq = 440.0f; // Not a valid note, default to A

	for (rx = -1; rx <= 1; rx++)
	for (ry = -1; ry <= 1; ry++)
		if (BOUNDS_CHECK) {
			r = pmap[y + ry][x + rx];
			if (!r) continue;
			rt = TYP(r);

			// Sparked, play note!
			if (rt == PT_SPRK || ((rx == 0 || ry == 0) && rt == PT_BRAY)) {
				NOTE::get_sound()->add_sound(freq, parts[i].life, parts[i].tmp2);
				return 0;
			}
		}

	return 0;
}
