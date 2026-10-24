#include "simulation/ElementCommon.h"
#include "simulation/quantum/quantum.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_ION() {
	Identifier = "DEFAULT_PT_ION";
	Name = "ION";
	Colour = PIXPACK(0x80DBFF);
	MenuVisible = 1;
	MenuSection = SC_NUCLEAR;
	Enabled = 1;
	Weight = -1;

	Collision = -0.99f;
	DefaultProperties.temp = 50.0f;
	Properties = TYPE_ENERGY;

	HeatConduct = 251;
	Description = "Supercooled ion, can be used for quantum computing. Decoheres with heat and interactions.";

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * ION stores a state id, which is the quantum state its linked to, inside
	 * of its tmp2 variable. Most ION interactions occur in QLOG
	 */

	int r, rx, ry;
	bool collided = false;

	for (rx=-1; rx<2; rx++)
		for (ry=-1; ry<2; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y+ry][x+rx];
				if (!r) r = sim->photons[y + ry][x + rx];
				if (!r) continue;

				// Decohere when touching other particles
				if (TYP(r) != PT_QLOG && TYP(r) != PT_ION && TYP(r) != PT_FILL) {
					// Collision, reverse other particle velocity
					// Yes this violates conservation of momentum
					parts[i].vx = parts[ID(r)].vx;
					parts[i].vy = parts[ID(r)].vy;
					parts[ID(r)].vx = -parts[ID(r)].vx;
					parts[ID(r)].vy = -parts[ID(r)].vy;
					
					// Make quantum circuits easier to build by
					// not having decoherence or dying when diagonal
					// pixel is touching
					if (parts[i].flags > 0 && (rx == 0 || ry == 0)) {
						if (RNG::Ref().chance(1, 20))
							QUANTUM::decohere_particle(parts, i);
						if (RNG::Ref().chance(1, 800))
							sim->kill_part(i);
					}
					collided = true;
				}
			}

	// Randomly diffuse
	if (parts[i].temp > 20.0f) {
		// If overheated will impart a random velocity to still IONs
		if (!collided && parts[i].vx == 0 && parts[i].vy == 0) {
			parts[i].vx = (RNG::Ref().uniform01() * 8.0f + 1.0f) * (RNG::Ref().chance(1, 2) ? -1 : 1);
			parts[i].vy = (RNG::Ref().uniform01() * 8.0f + 1.0f) * (RNG::Ref().chance(1, 2) ? -1 : 1);
		}

		// Randomly decohere
		if (parts[i].flags > 0 && RNG::Ref().chance(1, 30))
			QUANTUM::decohere_particle(parts, i);

		// Randomly diffuse
		if (RNG::Ref().chance(1, 10)) {
			parts[i].vx += RNG::Ref().chance(1, 2) ? -1 : 1;
			parts[i].vy += RNG::Ref().chance(1, 2) ? -1 : 1;
		}
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*firea = 30;
	*firer = *colr;
	*fireg = *colg;
	*fireb = *colb;

	*pixel_mode &= ~PMODE_FLAT;
	*pixel_mode |= FIRE_ADD | PMODE_ADD;
	if (RNG::Ref().chance(1, 1000)) // Don't flare too much
		*pixel_mode |= PMODE_LFLARE;

	return 0;
}
