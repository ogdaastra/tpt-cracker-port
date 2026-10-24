#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element::Element_NAVI()
{
	Identifier = "DEFAULT_PT_NAVI";
	Name = "NAVI";
	Colour = PIXPACK(0xd000ff);
	MenuVisible = 1;
	MenuSection = SC_SPACE; // SC_SPACE in TPT Ultimata; SC_SPECIAL before the menu split
	Enabled = 1;

	// element properties here

	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS)
{
	// update code here

	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	// graphics code here
	// return 1 if nothing dymanic happens here

	return 0;
}



