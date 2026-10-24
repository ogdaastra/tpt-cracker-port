#include "simulation/ToolCommon.h"
#include "simulation/ElementCommon.h"

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength);

void SimTool::Tool_DECO() {
	Identifier = "DEFAULT_TOOL_DECO";
	Name = "DECO";
    Colour = PIXPACK(0xba85ff);
    Description = "Sets the dcolour of non-decorated particles to its element color.";
	Perform = &perform;
}

static int perform(Simulation * sim, Particle * cpart, int x, int y, int brushX, int brushY, float strength) {
	if (cpart && cpart->type) {
		int cr, cg, cb, ca, ct, useless;
		if (sim->elements[cpart->type].Graphics) {
			if (sim->ren->graphicscache[cpart->type].isready) {
				cr = sim->ren->graphicscache[cpart->type].colr / 255.0f * sim->ren->graphicscache[cpart->type].cola;
				cg = sim->ren->graphicscache[cpart->type].colg / 255.0f * sim->ren->graphicscache[cpart->type].cola;
				cb = sim->ren->graphicscache[cpart->type].colb / 255.0f * sim->ren->graphicscache[cpart->type].cola;
			}
			else {
				ct = sim->elements[cpart->type].Colour;
				cr = PIXR(ct), cg = PIXG(ct), cb = PIXB(ct), ca = 255;
				(*(sim->elements[cpart->type].Graphics))(sim->ren,
					cpart, cpart->x + 0.5f, cpart->y + 0.5f, &useless, &ca, &cr, &cg, &cb,
						&useless, &useless, &useless, &useless);
				cr = cr / 255.0f * ca;
				cg = cg / 255.0f * ca;
				cb = cb / 255.0f * ca;
			}
		} else {
			ct = sim->elements[cpart->type].Colour;
			cr = PIXR(ct), cg = PIXG(ct), cb = PIXB(ct);
		}

		cr = std::min(255, std::max(cr, 0));
		cg = std::min(255, std::max(cg, 0));
		cb = std::min(255, std::max(cb, 0));
        cpart->dcolour = 0xFF000000 | PIXRGB(cr, cg, cb);
	}
	return 1;
}
