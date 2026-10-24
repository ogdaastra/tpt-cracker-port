#include "simulation/ElementCommon.h"
#include "simulation/vehicles/horse.h"
#include "simulation/vehicles/vehicle.h"

#include "simulation/FixedUtils.h"

#include <iostream>

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void create(ELEMENT_CREATE_FUNC_ARGS);
static void changeType(ELEMENT_CHANGETYPE_FUNC_ARGS);

int  Element_CYTK_create_part(Simulation *sim, int x, int y, int type, float theta, Particle *parts, int i);
void Element_CYTK_get_player_command(Simulation *sim, Particle *parts, int i, int &cmd, int &cmd2);
void Element_CYTK_initial_collision(Simulation *sim, Particle *parts, int i, const Vehicle &v, bool &has_collision);
void Element_CYTK_get_target(Simulation *sim, Particle *parts, int &tarx, int &tary);
void Element_CYTK_exit_vehicle(Simulation *sim, Particle *parts, int i, int x, int y);
void Element_CYTK_update_vehicle(Simulation *sim, Particle *parts, int i, const Vehicle &v, float ovx, float ovy);

void Element::Element_HRSE() {
	Identifier = "DEFAULT_PT_HRSE";
	Name = "HRSE";
	Colour = PIXPACK(0xC27536);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_ORGANIC in TPT Ultimata
	Enabled = 1;

	Advection = 0.01f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.9999f;
	Collision = -0.1f;
	Gravity = 0.1f;
	Diffusion = 0.0f;
	HotAir = 0.00f * CFDS;
	Falldown = 1;

	Flammable = 1;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;
	Weight = 50;

	DefaultProperties.temp = R_TEMP + 14.6f + 273.15f;
	HeatConduct = 60;
	Description = "Horse. Can be processed into GLUE, STKM can ride it, press down to dismount.";

	Properties = TYPE_PART | PROP_NOCTYPEDRAW | PROP_VEHICLE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 620.0f;
	HighTemperatureTransition = PT_FIRE;

	FixedMultiplier = 100000000; // 8

	DefaultProperties.life = 100;
	DefaultProperties.tmp4 = (int)(-PI / 4 * 100000000); // About 45 deg upwards default neck rotation

	Update = &update;
	Graphics = &graphics;
	Create = &create;
	ChangeType = &changeType;
}

static void create(ELEMENT_CREATE_FUNC_ARGS) {
	if (RNG::Ref().chance(1, 2))
		sim->parts[i].tmp |= 2; // Randomize direction it's facing
}

static void changeType(ELEMENT_CHANGETYPE_FUNC_ARGS) {
	if (to == PT_NONE && sim->parts[i].life <= 0) {
		int rx, ry, r;
		for (rx = -2; rx <= 2; rx++)
		for (ry = -2; ry <= 2; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = sim->pmap[y + ry][x + rx];
				if (!r) {
					sim->create_part(-1, x + rx, y + ry, PT_DUST);
					continue;
				}
				// Water -> GLUE
				if (sim->elements[TYP(r)].Properties & PROP_WATER && sim->parts[ID(r)].temp > 273.15f + 50.0f)
					sim->part_change_type(ID(r), x + rx, y + ry, PT_GLUE);
			}
	}
}

static int update(UPDATE_FUNC_ARGS) {
	// NOTE: HORSE UPDATES TWICE PER FRAME
	/* Properties:
	 * vx, vy (velocity)
	 * ctype = element of STKM when it entered
	 * tmp2 = which STKM controls it (0 = AI horse, 1 = STKM, 2 = STK2, 3 = FIGH)
	 * tmp | 1 = STKM rocket boot state or such
	 * tmp | 2 = Direction of travel (true = right, false = left)
	 * tmp | 4 = Fleeing to left
	 * tmp | 8 = Fleeing to right
	 * life = health
	 * tmp3 = rotation
	 * tmp4 = neck rotation
	 */
	float ovx = parts[i].vx, ovy = parts[i].vy;
	int rx, ry, r;
	bool has_collision;
	Element_CYTK_initial_collision(sim, parts, i, Horse, has_collision);

	float bodyAngle = FROM_FIXED_E(parts[i].tmp3, PT_HRSE);

	// Self destruction
	if (parts[i].life <= 0) {
		sim->kill_part(i);
		return 0;
	}

	// Heat damage
	if (parts[i].temp > 273.15f + 50.0f)
		parts[i].life -= 2;
	if (parts[i].temp > 273.15f + 100.0f)
		sim->kill_part(i);
	// Pressure damage
	if (fabs(sim->pv[y / CELL][x / CELL]) > 1.0f)
		parts[i].life -= 2;

	// If life <= 25 drip BLOD
	if (parts[i].life <= 25) {
		sim->create_part(-1, x, y + 1, PT_BLOD);
		parts[i].life--;
	}

	int cmd = NOCMD, cmd2 = NOCMD;
	// Fleeing from danger
	if (parts[i].tmp & 4) {
		cmd = LEFT;
		if (RNG::Ref().chance(1, 150))
			parts[i].tmp &= ~4;
	}
	else if (parts[i].tmp & 8) {
		cmd = RIGHT;
		if (RNG::Ref().chance(1, 150))
			parts[i].tmp &= ~8;
	}
	
	// Horse AI
	else if (parts[i].tmp2 == 0) {
		// Eat stuff
		int foodx = Horse.width / 2, foody = Horse.height / 2;
		if (parts[i].tmp & 2) foodx = -foodx - 5; // 5 is correction offset for some reason
		rotate(foodx, foody, bodyAngle);
		bool found_food = false;

		for (rx = -2; rx <= 2; rx++)
		for (ry = -2; ry <= 2; ry++)
			if (BOUNDS_CHECK && (x + foodx + rx >= 0 && x + foodx + rx < XRES && y + foody + ry >= 0 && y + foody + ry < YRES)) {
				r = pmap[y + foody + ry][x + foodx + rx];
				if (r && sim->elements[TYP(r)].Properties & PROP_EDIBLE) {
					parts[i].tmp4 = 0;
					found_food = true;
					if (RNG::Ref().chance(1, 200)) {
						parts[i].life += sim->elements[TYP(r)].FoodValue;
						sim->kill_part(ID(r));
					}
				}
			}
		if (!found_food) {
			parts[i].tmp4 = TO_FIXED_E(-PI / 4, PT_HRSE); // Normal neck angle
			if (RNG::Ref().chance(1, 200))
				cmd = RNG::Ref().chance(1, 2) ? 1 : 2;
		}
	}

	// Player controls
	else if (is_stkm(parts[i].tmp2))
		Element_CYTK_get_player_command(sim, parts, i, cmd, cmd2);
	// Fighter AI
	else if (is_figh(parts[i].tmp2)) {
		int tarx = -1, tary = -1; // Get target and run towards it
		Element_CYTK_get_target(sim, parts, tarx, tary);
		if (tarx > 0 && has_collision)
			cmd = tarx > parts[i].x ? RIGHT : LEFT;
	}

	// Horse instinct that overrides any player controls
	for (rx = -1; rx <= 1; rx++)
	for (ry = -1; ry <= 1; ry++)
		if (BOUNDS_CHECK && (rx || ry)) {
			// Run away from hot air
			// Run away from pressure
			if (fabs(sim->pv[y / CELL + ry][x / CELL + rx]) > 0.2f ||
					fabs(sim->hv[y / CELL + ry][x / CELL + rx]) > 273.15f + 60.0f)
				goto end;
		}
	for (rx = -5; rx <= 5; rx++)
	for (ry = -5; ry <= 5; ry++)
		if ((x + rx < XRES && x + rx >= 0 && y + ry >= 0 && y + ry < YRES) && (rx || ry)) {
			r = pmap[y + ry][x + rx];
			if (!r) r = sim->photons[y + ry][x + rx];
			if (!r) continue;

			// Run away from hot things
			// Run away from DEADLY things
			if (parts[ID(r)].temp > 273.15f + 60.0f || sim->elements[TYP(r)].Properties & PROP_DEADLY)
				goto end;
		}
	if (false) {
		end:;
		parts[i].life -= 3;
		parts[i].tmp &= ~4;
		parts[i].tmp &= ~8;
		if (rx > 0) parts[i].tmp |= 4;
		if (rx < 0) parts[i].tmp |= 8;
		cmd = rx > 0 ? LEFT : RIGHT;
	}

	// Do controls
	if (cmd != NOCMD || cmd2 != NOCMD) {
		if (has_collision || (parts[i].tmp & 1)) { // Accelerating only can be done on ground or if rocket
		 	float ax = has_collision ? -Horse.acceleration : -Horse.fly_acceleration / 8.0f, ay = 0.0f;
			if (cmd == LEFT) { // Left
				rotate(ax, ay, bodyAngle);
				parts[i].vx += ax, parts[i].vy += ay;
				parts[i].tmp |= 2; // Set face direction
				parts[i].y -= 0.5;
			}
			else if (cmd == RIGHT) { // Right
				ax *= -1;
				rotate(ax, ay, bodyAngle);
				parts[i].vx += ax, parts[i].vy += ay;
				parts[i].tmp &= ~2; // Set face direction
				parts[i].y -= 0.5;
			}
		}
		if (cmd2 == DOWN) { // Exit (down)
			Element_CYTK_exit_vehicle(sim, parts, i, x, y);
			return 0;
		}
		else if (cmd2 == UP) { // Jump
			float ax = 0.0f, ay = 0.0f;
			if (parts[i].tmp & 1) {
				ay = -Horse.fly_acceleration / 4.0f;
				int j1 = Element_CYTK_create_part(sim, Horse.width * 0.4f, Horse.height / 2, PT_PLSM, bodyAngle, parts, i);
				int j2 = Element_CYTK_create_part(sim, -Horse.width * 0.4f, Horse.height / 2, PT_PLSM, bodyAngle, parts, i);
				if (j1 > -1 && j2 > -1) {
					parts[j1].temp = parts[j2].temp = 400.0f;
					parts[j1].life = RNG::Ref().between(0, 100) + 50;
					parts[j2].life = RNG::Ref().between(0, 100) + 50;
				}
			}
			else if (has_collision) {
				ay -= 10;
			}
			rotate(ax, ay, bodyAngle);
			parts[i].vx += ax;
			parts[i].vy += ay;
		}
	}

	Element_CYTK_update_vehicle(sim, parts, i, Horse, ovx, ovy);
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*colr = *colg = *colb = 255;
	draw_horse(ren, cpart, cpart->vx, cpart->vy);
	return 0;
}

