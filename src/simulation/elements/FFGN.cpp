#include "simulation/ElementCommon.h"
#include <set>

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

namespace FFGN {
	// Keep track when to clear
	unsigned int previous_sim_timer = 0;
	const int MAX_LINE_DIS = 100 * 100; // 100 px max FFGN connection, 100^2 = 100 * 100
	std::set<int> line_generators;
}

void Element::Element_FFGN() {
	Identifier = "DEFAULT_PT_FFGN";
	Name = "FFGN";
	Colour = PIXPACK(0x7F6600);
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
	Hardness = 1;
	
	Weight = 100;
	
	DefaultProperties.temp = 100.0f;
	HeatConduct = 0;
	Description = "Force field generator. HEAT / COOL = size, tmp = shape, tmp2 = type. Use sparingly.";

	Properties = TYPE_SOLID | PROP_NOAMBHEAT | PROP_NOCTYPEDRAW;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = 100.8f;
	HighPressureTransition = PT_BREC;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITL;
	HighTemperatureTransition = NT;

	Update = &update;
	Graphics = &graphics;
	CtypeDraw = &Element::ctypeDrawVInTmp;
}

static void flood_fill_FFLD(Particle *parts, Simulation *sim, int i, int x, int y) {
	if (x < 0 || y < 0 || x >= XRES || y >= YRES)
		return;

	PropertyValue value1, value2, value3;
	value1.Integer = parts[i].tmp;
	value2.Integer = parts[i].ctype;
	value3.UInteger = parts[i].dcolour;
	sim->flood_prop(x, y, offsetof(Particle, tmp), value1, StructProperty::Integer);
	sim->flood_prop(x, y, offsetof(Particle, ctype), value2, StructProperty::Integer);
	sim->flood_prop(x, y, offsetof(Particle, dcolour), value3, StructProperty::UInteger);
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Props:
	 * temp: radius / size
	 * ctype: type to affect
	 * life: on / off state
	 * tmp: shape
	 *   0: circle
	 *   1: square
	 *   2: connect to nearest FFGN
	 * tmp2: type
	 *   See FFLD.cpp
	 * flags:
	 * 	  How many FFGN surrounds it, used for ring graphics
	 */

	// Every 10 frames recalculate
	bool recalculate = sim->timer % 100 == 0 && FFGN::previous_sim_timer != sim->timer;
	if (recalculate) {
		FFGN::previous_sim_timer = sim->timer;
		FFGN::line_generators.clear();
	}

	// Add self to corresponding data arrays
	if (parts[i].tmp == 2)
		FFGN::line_generators.insert(i);

	parts[i].flags = 0; // Reset every frame

	if (parts[i].life > 0) {
		if (parts[i].tmp == 0) { // Circle
			int prevx = -1, prevy = -1;
			for (float angle = 0.0f; angle < 2 * 3.141592f; angle += 0.001f){
				int x1 = sin(angle) * parts[i].temp + parts[i].x;
				int y1 = cos(angle) * parts[i].temp + parts[i].y;

				if (x1 == prevx && y1 == prevy)
					continue;

				int ni = sim->create_part(-1, x1, y1, PT_FFLD);
				if (ni > -1) {
					parts[ni].ctype = parts[i].ctype;
					parts[ni].tmp2 = parts[i].tmp2;
					parts[ni].dcolour = parts[i].dcolour;
				}
				prevx = x1, prevy = y1;
			}
		}
		else if (parts[i].tmp == 1) { // Square
			sim->CreateBox(x - parts[i].temp, y - parts[i].temp, x + parts[i].temp, y - parts[i].temp, PT_FFLD, 0);
			sim->CreateBox(x - parts[i].temp, y + parts[i].temp, x + parts[i].temp, y + parts[i].temp, PT_FFLD, 0);
			sim->CreateBox(x - parts[i].temp, y - parts[i].temp, x - parts[i].temp, y + parts[i].temp, PT_FFLD, 0);
			sim->CreateBox(x + parts[i].temp, y - parts[i].temp, x + parts[i].temp, y + parts[i].temp, PT_FFLD, 0);

			// Find a middle of a side to floodfill from
			flood_fill_FFLD(parts, sim, i, x, y + parts[i].temp);
			flood_fill_FFLD(parts, sim, i, x, y - parts[i].temp);
			flood_fill_FFLD(parts, sim, i, x + parts[i].temp, y);
			flood_fill_FFLD(parts, sim, i, x - parts[i].temp, y);
		}
		else if (parts[i].tmp == 2 && FFGN::line_generators.size() >= 2) { // Electrode like behavior!
			// Find nearest 2 neighbours and draw lines to them
			int dis1 = 999999999, dis2 = 999999999;
			int id1 = 0, id2 = 0;
			for (auto j = FFGN::line_generators.begin(); j != FFGN::line_generators.end(); ++j) {
				if (*j >= i)
					continue;
				int tdis = (parts[*j].x - parts[i].x) * (parts[*j].x - parts[i].x) +
						   (parts[*j].y - parts[i].y) * (parts[*j].y - parts[i].y);
				if (tdis < dis1)
					id1 = *j, dis1 = tdis;
				else if (tdis < dis2)
					id2 = *j, dis2 = tdis;
			}

			// Only draw if within 100 px (100^2 = 10000)
			if (i > id1 && dis1 < FFGN::MAX_LINE_DIS)
				sim->CreateLine(x, y, parts[id1].x, parts[id1].y, PT_FFLD);
			if (i > id2 && dis2 < FFGN::MAX_LINE_DIS)
				sim->CreateLine(x, y, parts[id2].x, parts[id2].y, PT_FFLD);

			flood_fill_FFLD(parts, sim, i, x + 1, y);
			flood_fill_FFLD(parts, sim, i, x - 1, y);
			flood_fill_FFLD(parts, sim, i, x, y - 1);
			flood_fill_FFLD(parts, sim, i, x, y + 1);
		}
	}

	// Check for ON / OFF
	int r, rx, ry, rt;
	for (rx = -1; rx < 2; rx++)
		for (ry = -1; ry < 2; ry++)
			if (BOUNDS_CHECK) {
				r = pmap[y+ry][x+rx];
				if (!r) continue;
				rt = TYP(r);

				if (rt == PT_FFGN)
					parts[i].flags++;
				else if (rt == PT_SPRK && (parts[ID(r)].ctype == PT_PSCN || parts[ID(r)].ctype == PT_NSCN)) {
					PropertyValue value;
					value.Integer = parts[ID(r)].ctype == PT_PSCN ? 1 : 0;
					sim->flood_prop(parts[i].x, parts[i].y, offsetof(Particle, life), value, StructProperty::Integer);
				}
			}


	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	// Lighter if activated
	if (cpart->life > 0) {
		*colr *= 2;
		*colg *= 2;
		*colb *= 2;

		// Fancy rings emitting from the generator
		// Ring lasts 100 frames
		// Only draw if 3 or less surround generators
		if (cpart->flags <= 3) {
			float multi = (ren->sim->timer % 100) / 100.0f; 
			if (cpart->tmp == 0) { // Circle
				for (float angle = 0.0f; angle < 2 * 3.141592f; angle += 0.06f){
					int x1 = multi * sin(angle) * cpart->temp + cpart->x;
					int y1 = multi * cos(angle) * cpart->temp + cpart->y;
					int x2 = multi * sin(angle + 0.06f) * cpart->temp + cpart->x;
					int y2 = multi * cos(angle + 0.06f) * cpart->temp + cpart->y;
					ren->draw_line(x1, y1, x2, y2, 200, 200, 255, 100);
				}
			}
			else if (cpart->tmp == 1) { // Square
				ren->draw_line(cpart->x - cpart->temp * multi, cpart->y - cpart->temp * multi, cpart->x - cpart->temp * multi, cpart->y + cpart->temp * multi, 200, 200, 255, 100);
				ren->draw_line(cpart->x + cpart->temp * multi, cpart->y - cpart->temp * multi, cpart->x + cpart->temp * multi, cpart->y + cpart->temp * multi, 200, 200, 255, 100);
				ren->draw_line(cpart->x + cpart->temp * multi, cpart->y - cpart->temp * multi, cpart->x - cpart->temp * multi, cpart->y - cpart->temp * multi, 200, 200, 255, 100);
				ren->draw_line(cpart->x + cpart->temp * multi, cpart->y + cpart->temp * multi, cpart->x - cpart->temp * multi, cpart->y + cpart->temp * multi, 200, 200, 255, 100);
			}
		}
	}

	return 0;
}
