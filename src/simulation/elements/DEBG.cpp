#include "simulation/ElementCommon.h"

#include "simulation/circuits/circuits.h"
#include "simulation/circuits/framework.h"
#include "simulation/circuits/resistance.h"
#include "simulation/magnetics/magnetics.h"

#include <unordered_map>
#include <iostream>

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void changeType(ELEMENT_CHANGETYPE_FUNC_ARGS);

namespace DEBG_DATA {
	const int MAX_CIRCUIT_DATA = 300;

	class DebugData {
	public:
		std::vector<float> prev_voltages; // Should be a list but fuck this
		std::vector<float> prev_currents;
		DebugData() {}
	};

	std::unordered_map<int, DebugData> data = {};
};

void Element::Element_DEBG() {
	Identifier = "DEFAULT_PT_DEBG";
	Name = "DEBG";
	Colour = PIXPACK(0xFF0000);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_CRACKER2 in TPT Ultimata
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

	HeatConduct = 0;
	Description = "Debugger (for mod testing lol). Set tmp to change debugging mode.";

	Properties = TYPE_SOLID | PROP_INDESTRUCTIBLE;

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
	ChangeType = &changeType;
}

static void changeType(ELEMENT_CHANGETYPE_FUNC_ARGS) {
	DEBG_DATA::data.erase(i);
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Debug types:
	 * 1 - Get faraday wall ID
	 * 2 - List circuits
	 * 3 - Graph voltage and current
	 * 4 - Instantly crash with float point exception
	 */
	if (parts[i].tmp == 3) {
		for (int rx = -1; rx <= 1; rx++)
		for (int ry = -1; ry <= 1; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				int r = sim->photons[y + ry][x + rx];
				if (!r || TYP(r) != PT_RSPK) continue;
				auto p = parts[ID(r)];

				DEBG_DATA::data[i].prev_voltages.push_back(p.tmp3);
				DEBG_DATA::data[i].prev_currents.push_back(p.tmp4);

				if (DEBG_DATA::data[i].prev_voltages.size() > DEBG_DATA::MAX_CIRCUIT_DATA) {
					DEBG_DATA::data[i].prev_voltages.erase(DEBG_DATA::data[i].prev_voltages.begin());
					DEBG_DATA::data[i].prev_currents.erase(DEBG_DATA::data[i].prev_currents.begin());
				}
				break;
			}
	}
	else if (parts[i].tmp == 4) {
		parts[i].tmp2 = 5 % (x - x);
	}

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	if (cpart->tmp == 1)
		ren->drawtext(nx, ny - 5, String::Build("ID: ", ren->sim->faraday_map[ny / CELL][nx / CELL]), 255, 255, 255, 255);
	else if (cpart->tmp == 2) {
		int tx = ren->mousePos.X,
			ty = ren->mousePos.Y;
		ren->drawtext(nx, ny, String::Build(all_circuits.size(), circuit_map[ID(ren->sim->photons[ty][tx])] != nullptr ? 1 : 0), 255, 255, 255, 255);

		int y = ny + 12;
		for (auto c : all_circuits) {
			ren->drawtext(nx, y, String::Build("Branches: ", c->branch_cache_size()), 255, 255, 255, 255);
			y += 12;
		}
	}
	else if (cpart->tmp == 3) {
		int i = ID(ren->sim->pmap[ny][nx]);
		int sx = nx, sy1 = ny, sy2 = ny;
		int cx, cy1, cy2;

		for (size_t j = 0; j < DEBG_DATA::data[i].prev_currents.size(); j++) {
			cx = nx + j + 1;
			cy1 = ny - DEBG_DATA::data[i].prev_voltages[j] * 10;
			cy2 = ny - DEBG_DATA::data[i].prev_currents[j] * 100;

			ren->draw_line(sx + 10, sy1 - 10, cx, cy1 - 10, 255, 0, 0, 255);
			ren->draw_line(sx + 10, sy2 - 10, cx, cy2 - 10, 255, 255, 0, 255);
			sx = cx;
			sy1 = cy1;
			sy2 = cy2;
		}
	}
	return 0;
}
