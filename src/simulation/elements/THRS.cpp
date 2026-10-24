#include "simulation/ElementCommon.h"
#include "simulation/Spaceship.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element_HULL_create(ELEMENT_CREATE_FUNC_ARGS);
int Element_HULL_update(UPDATE_FUNC_ARGS);
void Element_HULL_changeType(ELEMENT_CHANGETYPE_FUNC_ARGS);

#include <cmath>
#include <vector>


void Element::Element_THRS() {
	Identifier = "DEFAULT_PT_THRS";
	Name = "THRS";
	Colour = PIXPACK(0x030163);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_SPACE in TPT Ultimata
	Enabled = 1;

	// element properties here

	Update = &update;
	Graphics = &graphics;
	Create = &Element_HULL_create;
	ChangeType = &Element_HULL_changeType;
}

static int update(UPDATE_FUNC_ARGS) {
	// Clone tmp3 and ship state
	Element_HULL_update(sim, i, x, y, surround_space, nt, parts, pmap);
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	// graphics code here
	// return 1 if nothing dymanic happens here

	return 0;
}



