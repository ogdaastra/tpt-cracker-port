#include "simulation/ElementCommon.h"
#include "simulation/Spaceship.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);
static void changeType(ELEMENT_CHANGETYPE_FUNC_ARGS);

#include <climits>
#include <vector>

void Element::Element_COTR()
{
	Identifier = "DEFAULT_PT_COTR";
	Name = "COTR";
	Colour = PIXPACK(0x5c0e0e);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_SPACE in TPT Ultimata
	Enabled = 1;


	// element properties here
	Update = &update;
	Graphics = &graphics;
	Create = &create;
	ChangeType = &changeType;
}

static void create(ELEMENT_CREATE_FUNC_ARGS) {
	sim->parts[i].tmp3 = -1;
}

static void changeType(ELEMENT_CHANGETYPE_FUNC_ARGS) {
	// Remove self if dead
	if (SHIPS::ships.count(sim->parts[i].tmp3) > 0)
		SHIPS::ships[sim->parts[i].tmp3].destroy(sim); // Ship self-destructs if controller dies
}

static int update(UPDATE_FUNC_ARGS)
{
	// tmp3 is unique ship ID
	// Generate ship ID or regenerate if it doesn't exist
	if (parts[i].tmp3 < 0 || !SHIPS::is_ship(parts[i].tmp3)) {
		parts[i].tmp3 = SHIPS::create_ship(parts[i].tmp3);
		SHIPS::ships[parts[i].tmp3].add_component(i, parts[i].type);
	}

	SHIPS::move_ship(sim, parts[i].tmp3);
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS)
{
	// graphics code here
	// return 1 if nothing dymanic happens here
	return 0;
}



