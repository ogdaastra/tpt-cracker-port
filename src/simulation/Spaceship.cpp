#include "simulation/ElementCommon.h"
#include "simulation/Spaceship.h"

#include <algorithm>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <iostream>
#include <set>

/**
Properties:
	tmp3 - Storing ship id in componenets connected to a COTR
*/

namespace SHIPS {
	std::unordered_map<int, Spaceship> ships;

	// Is a type a component? Pass a predefined value like PT_THRS
	bool is_component(int comp) {
		return std::find(components.begin(), components.end(), comp) != components.end();
	}

	// Is a ship id valid
	bool is_ship(int id) {
		return ships.count(id);
	};

	// Generate a random unused ship id in ships
	int generate_ship_id() {
		int id;
		do {
			id = RNG::Ref().between(0, 10000000);
			if (!is_ship(id))
				return id;
		} while (true);
	}


	// ------- Ship ------------------

	// Create a ship
	int create_ship(int id) {
		id = id < 0 ? generate_ship_id() : id;
		ships[id] = Spaceship(id);
		return id;
	}

	// Ship self destruction, reset all ship IDs
	void Spaceship::destroy(Simulation* sim) {
		for (auto i = components.begin(); i != components.end(); ++i) {
			if (is_component(sim->parts[*i].type) && sim->parts[*i].tmp3 == id)
				sim->parts[*i].tmp3 = -1;
		}
	}

	// ------ Movement -------

	// Net ship acceleration computation
	void calculate_acceleration(Simulation *sim, int id, float &nax, float &nay) {
		nax = 0.0f, nay = 0.0f;
		int rx, ry, r, tx, ty, c;

		for (auto i = ships[id].component_map[PT_THRS].begin(); i != ships[id].component_map[PT_THRS].end(); ++i) {
			// tx, ty = current thruster output direction. c = number of directions it outputs in. Ie
			// a thruster in the following arrangment (T = THRS, X = non-empty space)
			// 	X X X
			// 	X T
			// 	X
			// has 3 direcitons it can output (3 surrounding empty spaces)
			tx = ty = c = 0;
			for (int rx = -1; rx <= 1; ++rx)
			for (int ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = sim->pmap[(int)(ry + sim->parts[*i].y + 0.5f)][(int)(rx + sim->parts[*i].x + 0.5f)];
				if (!r) tx -= rx, ty -= ry, ++c;
			}

			// Divide the net thrust by number of directions
			if (c) nax += (float)tx / c, nay += (float)ty / c;
		}
		nax *= THRUSTER_FORCE, nay *= THRUSTER_FORCE;

		// Float point error correction
		if (fabs(nax) < 0.01) nax = 0.0f;
		if (fabs(nay) < 0.01) nay = 0.0f;

		// Correct acceleration by dividing force by ship mass
		nax /= ships[id].components.size();
		nay /= ships[id].components.size();
	}

	// Apply movement
	void move_ship(Simulation* sim, int id) {
		if (!is_ship(id)) // Ship ID invalid!
			return;

		float nax, nay;
		calculate_acceleration(sim, id, nax, nay);
		ships[id].vx += nax;
		ships[id].vy += nay;

		// Collision check: does moving cause a particle to go inside another particle?

		// Apply movement to each component
		for (auto i = ships[id].components.begin(); i != ships[id].components.end(); ++i) {
			sim->parts[*i].x += ships[id].vx;
			sim->parts[*i].y += ships[id].vy;
		}
	}


	// --------- Sim -------------------
	void cloneTMP34(Simulation* sim, int i, int x, int y) {
		// Dont scan if already has a ship ID
		if (sim->parts[i].tmp3 >= 0 && is_ship(sim->parts[i].tmp3))
			return;
		int r, rx, ry;
		for (rx = -1; rx <= 1; rx++)
		for (ry = -1; ry <= 1; ry++)
			if (BOUNDS_CHECK && (rx || ry)) { // Scan around for other components
				r = sim->pmap[y + ry][x + rx];
				if (!r || sim->parts[ID(r)].tmp3 < 0) continue; // Skip check if doesn't exist or other component doesn't have an id either
				if (is_ship(sim->parts[ID(r)].tmp3)) { // If ship id exists we can add it to this component
					// Checks if component adjacent is a component type or if it's pointer is
					if (is_component(sim->parts[ID(r)].type)) {
						sim->parts[i].tmp3 = sim->parts[ID(r)].tmp3;
						ships[sim->parts[i].tmp3].add_component(i, sim->parts[i].type);  // Add component to ship
						return;
					}
				}
			}
	}
}
