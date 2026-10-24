#include "simulation/ElementCommon.h"
#include <queue>

static int update(UPDATE_FUNC_ARGS);

#define MAX_RCRT_CONNECT 5

// Floodfills connecting RCRT to tmp2 = 5, only does this for nearest 5
void floodfill_valid_connect(Simulation *sim, Particle *parts, int x, int y) {
	std::queue<std::pair<int, int> > queue;
	queue.push(std::make_pair(x, y));
	int flood_filled = 0;

	while (queue.size() && flood_filled < MAX_RCRT_CONNECT) {
		std::pair<int, int> p = queue.front();
		if (parts[ID(sim->pmap[p.second][p.first])].tmp2 == 5)
			goto loopend;
		parts[ID(sim->pmap[p.second][p.first])].tmp2 = 5;

		// Floodfill
		for (int rx = -1; rx <= 1; ++rx)
		for (int ry = -1; ry <= 1; ++ry)
		if ((rx || ry)) {
			int totype = TYP(sim->pmap[p.second + ry][p.first + rx]);
			if (totype == PT_RCRT)
				queue.push(std::make_pair(p.first + rx, p.second + ry));
		}

		loopend:
		queue.pop();
	}
}

void Element::Element_RCRT() {
	Identifier = "DEFAULT_PT_RCRT";
	Name = "RCRT";
	Colour = PIXPACK(0xA8A8A8);
	MenuVisible = 1;
	MenuSection = SC_POWDERS;
	Enabled = 1;

	Advection = 0.1f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.3f;
	Diffusion = 0.00f;
	HotAir = 0.000f	* CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 2;
	Hardness = 2;

	Weight = 90;
	HeatConduct = 100;
	Description = "Reinforced concrete. Remains in place near supporting solids.";

	Properties = TYPE_PART | PROP_HOT_GLOW;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 1223.0f;
	HighTemperatureTransition = PT_LAVA;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Properties:
	 * tmp  - How is it supported
	 * 		  1 = touching solid
	 * 		  2 = touching RCRT touching solid
	 * 		  3 = touching RCRT touching RCRT touching solid
	 * 		  ...
	 * Max support length is 5
	 * tmp2  - Is connected to a solid rn? Randomly resets, if not connected
	 * 		   Countsdown, then collapses
	 */
	int rx, ry, r, rt;
	int possible_connect_tmp = MAX_RCRT_CONNECT + 1; // For initial connecting
	bool found_solid = false;
	
	parts[i].tmp2--;
	if (parts[i].tmp2 < 0)
		parts[i].tmp2 = 0;

	for (rx = -1; rx <= 1; ++rx)
		for (ry = -1; ry <= 1; ++ry)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				if (sim->elements[rt].Properties & TYPE_SOLID || (rt == PT_ROCK && !parts[ID(r)].tmp2) ||
						(rt == PT_CRBN && parts[ID(r)].tmp2)) {
					parts[i].tmp = 1;
					found_solid = true;
				}
				else if (rt == PT_RCRT && parts[ID(r)].tmp && parts[i].tmp == 0) {
					if (possible_connect_tmp > parts[ID(r)].tmp + 1)
						possible_connect_tmp = parts[ID(r)].tmp + 1;
				}
			}

	if (possible_connect_tmp != MAX_RCRT_CONNECT + 1)
		parts[i].tmp = possible_connect_tmp;

	if (!found_solid && parts[i].tmp == 1) // No longer connected to a solid
		parts[i].tmp = MAX_RCRT_CONNECT + 1; // 0 means not yet calculated, 6 skips that

	// Floodfill connected to solid rn property
	if (parts[i].tmp == 1) {
		floodfill_valid_connect(sim, parts, x, y);
		// PropertyValue value;
		// value.Integer = 5;
		// sim->flood_prop(x, y, offsetof(Particle, tmp2), value, StructProperty::Integer);
	}
	
	if (parts[i].tmp > 0 && parts[i].tmp <= MAX_RCRT_CONNECT && parts[i].tmp2 > 0 &&
			sqrtf(parts[i].vx*parts[i].vx+parts[i].vy*parts[i].vy) < 2.0f) {
		parts[i].vx = parts[i].vy = 0.0f;
		return 1;
	}
	return 0;
}
