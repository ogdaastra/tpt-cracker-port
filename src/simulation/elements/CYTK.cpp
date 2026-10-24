#include "simulation/ElementCommon.h"
#include "simulation/vehicles/vehicle.h"
#include "simulation/vehicles/cybertruck.h"

#include "simulation/FixedUtils.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
void Element_CYTK_changeType(ELEMENT_CHANGETYPE_FUNC_ARGS);

void Element::Element_CYTK() {
	Identifier = "DEFAULT_PT_CYTK";
	Name = "CYTK";
	Colour = PIXPACK(0x4D5564);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_CRACKER2 in TPT Ultimata
	Enabled = 1;

	Advection = 0.01f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.15f; // NOTE: Cybertruck UPDATES TWICE PER FRAME
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 100;
	HeatConduct = 60;
	Description = "Tesla Cybertruck. STKM can ride, comes with several upgrades.";

	DefaultProperties.life = 100; // Default 100 HP

	Properties = TYPE_PART | PROP_NOCTYPEDRAW | PROP_VEHICLE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	FixedMultiplier = 100000000; // 8

	Update = &update;
	Graphics = &graphics;
	ChangeType = &Element_CYTK_changeType;
}

bool Element_CYTK_check_stkm(playerst *data, Simulation *sim, Particle *parts, int i) {
	int xdiff = abs(data->legs[0] - parts[i].x);
	int ydiff = abs(data->legs[1] - parts[i].y);
	if (xdiff + ydiff < MIN_STKM_DISTANCE_TO_ENTER) {
		if (parts[i].type != PT_HRSE) {
			if (data->elem == PT_FIRE) // Flamethrower
				parts[i].tmp = 2;
			else if (data->rocketBoots) // Cyberthruster
				parts[i].tmp = 1;
			else if (data->elem == PT_BOMB) // Bomb
				parts[i].tmp = 3;
		}
		else {
			if (data->rocketBoots)
				parts[i].tmp |= 1;
		}
		parts[i].ctype = data->elem;
		sim->kill_part(data->stkmID);
		return true;
	}
	return false;
}

bool Element_CYTK_attempt_move(int x, int y, Simulation *sim, Particle *parts, int i, const Vehicle &v) {
	if (parts[i].vx == 0 && parts[i].vy == 0)
		return false;

	// x, y are initially relative offsets when the vehicle is flat. Rotate and convert
	// to an absolute x y coordinate
	rotate(x, y, FROM_FIXED_E(parts[i].tmp3, PT_CYTK));
	x += parts[i].x;
	y += parts[i].y;
	// the rotated hull can reach past the screen edge; treat that as a wall (TPT's BOUNDS_CHECK is always true)
	if (x < 1 || y < 1 || x >= XRES - 1 || y >= YRES - 1) {
		parts[i].vx = parts[i].vy = 0;
		return true;
	}

	int r, rx, ry;

	// Search for nearby vehicles
	for (auto ov = sim->vehicles.begin(); ov != sim->vehicles.end(); ++ov) {
		if (parts[i].type == PT_HRSE) // Horses don't do collision
			break;
		if (*ov == i) // Dont collide with self
			continue;

		float dx = fabs(parts[i].x - parts[*ov].x);
		float dy = fabs(parts[i].y - parts[*ov].y);

		if (dx < v.width && dy < v.height) {
			if (fabs(parts[i].vx) > v.collision_speed / 2.0f || fabs(parts[i].vy) > v.collision_speed / 2.0f) {
				parts[*ov].life -= RNG::Ref().between(10, 50);
				parts[i].life   -= RNG::Ref().between(10, 50);
			}
			parts[i].vx = 0;
			parts[i].vy = 0;

			// Tank destroys other vehicles
			if (parts[i].type == PT_TANK && parts[*ov].type != PT_TANK)
				parts[*ov].life = 0;
			break;
		}
	}

	// Test surrounding particles
	for (rx = -1; rx < 2; rx++)
		for (ry = -1; ry < 2; ry++)
			if (BOUNDS_CHECK) {
				if (sim->IsWallBlocking(x + rx, y + ry, PT_CYTK)) {
					parts[i].vx = parts[i].vy = 0;
					return true;
				}

				r = sim->pmap[y + ry][x + rx];
				if (!r) continue;
				if (TYP(r) == PT_HRSE) continue;

				// Cybertrucks heal from electrons
				if (parts[i].type == PT_CYTK && TYP(r) == PT_ELEC) {
					parts[i].life += 5;
					if (parts[i].life > 120)
						parts[i].life = 120;
				}

				if (TYP(r) != PT_CYTK && TYP(r) != PT_PRTI && TYP(r) != PT_PRTO && TYP(r) != PT_TRUS &&
					(sim->elements[TYP(r)].Properties & TYPE_PART ||
					 sim->elements[TYP(r)].Properties & TYPE_SOLID)) {
					parts[i].vx = parts[i].vy = 0;
					return true;
				}
			}

	// Try moving in the direction of vx vy
	float largest = fabs(parts[i].vx) > fabs(parts[i].vy) ? fabs(parts[i].vx) : fabs(parts[i].vy);
	if (largest <= 1) { // Avoid "division by 0" effect where it ends up scanning the entire map
		return false;
	}
	float dvx = parts[i].vx / largest, dvy = parts[i].vy / largest;
	float sx = x, sy = y;
	int count = 1;

	while (sx >= 0 && sy >= 0 && sx < XRES && sy < YRES &&
			fabs(-dvx + dvx * count) <= fabs(parts[i].vx) && fabs(-dvy + dvy * count) <= fabs(parts[i].vy)) {
		r = sim->pmap[(int)round(sy)][(int)round(sx)];
		if (r) {
			if (TYP(r) != parts[i].type && TYP(r) != PT_PRTI && TYP(r) != PT_PRTO && TYP(r) != PT_TRUS &&
				(sim->elements[TYP(r)].Properties & TYPE_PART ||
				sim->elements[TYP(r)].Properties & TYPE_SOLID)) {
				parts[i].vx = parts[i].vy = 0;
				return true;
			}
		}

		sx += dvx; sy += dvy;
		count++;
	}
	return false;
}

int Element_CYTK_create_part(Simulation *sim, int x, int y, int type, float theta, Particle *parts, int i) {
	if (parts[i].tmp4)
		x = -x; // Car going other way
	rotate(x, y, theta);
	x += parts[i].x; y += parts[i].y;
	return sim->create_part(-1, x, y, type);
}

void Element_CYTK_get_player_command(Simulation *sim, Particle *parts, int i, int &cmd, int &cmd2) {
	int command = parts[i].tmp2 == 1 ? sim->player.comm : sim->player2.comm;
	if (((int)command & 0x03) == 0x03)      cmd = LEFT_AND_RIGHT;
	else if (((int)command & 0x01) == 0x01) cmd = LEFT;  // Left
	else if (((int)command & 0x02) == 0x02) cmd = RIGHT; // Right
	if (((int)command & 0x04) == 0x04) 		cmd2 = UP;   // Up
	else if (((int)command & 0x08) == 0x08) cmd2 = DOWN; // Down
}

void Element_CYTK_initial_collision(Simulation *sim, Particle *parts, int i, const Vehicle &v, bool &has_collision) {
	bool pbl = Element_CYTK_attempt_move(-v.width / 2, v.height / 2, sim, parts, i, v);
	bool pbr = Element_CYTK_attempt_move(v.width / 2, v.height / 2, sim, parts, i, v);
	bool pbc = Element_CYTK_attempt_move(0, v.height / 2, sim, parts, i, v);
	bool tbl = Element_CYTK_attempt_move(-v.width / 2, -v.height / 2, sim, parts, i, v);
	bool tbr = Element_CYTK_attempt_move(v.width / 2, -v.height / 2, sim, parts, i, v);
	bool tbc = Element_CYTK_attempt_move(0, -v.height / 2, sim, parts, i, v);
	has_collision = pbl || pbr || pbc || tbl || tbr || tbc;

	float angle = FROM_FIXED_E(parts[i].tmp3, parts[i].type);
	// Match terrain rotation
	if (pbl ^ pbr && !pbc) {
		angle += pbl ? v.rotation_speed : -v.rotation_speed;
		parts[i].y -= 0.5;
	}
	if (tbl ^ tbr && !tbc) {
		angle += tbl ? v.rotation_speed : -v.rotation_speed;
		parts[i].y += 0.5;
	}

	// If no collision rotate towards gravity every 10 frames
	if (!has_collision && sim->timer % 10 == 0)	{
		float target_angle = -1000.0f; // Placeholder
		switch (sim->gravityMode) {
		default:
		case 0: // Normal, vertical gravity
			target_angle = 0;
			break;
		case 1: // No gravity
			break;
		case 2: // Radial gravity
			target_angle = atan2(parts[i].y - YRES / 2, parts[i].x - XRES / 2) + 3.1415 / 2;
			break;
		}

		if (target_angle == -1000.0f) {}
		else if (angle > target_angle)
			angle -= v.rotation_speed;
		else
			angle += v.rotation_speed;
	}
	parts[i].tmp3 = TO_FIXED_E(angle, parts[i].type);

	// If sim stkm or stkm2 exists then stop allowing control to this car
	if (parts[i].tmp2 == 1 && sim->player.spwn)
		parts[i].tmp2 = 0;
	else if (parts[i].tmp2 == 2 && sim->player2.spwn)
		parts[i].tmp2 = 0;

	// Prevent spawning of stkm and stkm2 if inside car
	if (parts[i].tmp2 == 1)
		sim->vehicle_p1 = i;
	if (parts[i].tmp2 == 2)
		sim->vehicle_p2 = i;
}

void Element_CYTK_get_target(Simulation *sim, Particle *parts, int &tarx, int &tary) {
	if (sim->player.spwn)
		tarx = sim->player.legs[0], tary = sim->player.legs[1];
	else if (sim->player2.spwn)
		tarx = sim->player2.legs[0], tary = sim->player2.legs[1];
	else if (sim->vehicle_p1 >= 0)
		tarx = parts[sim->vehicle_p1].x, tary = parts[sim->vehicle_p1].y;
	else if (sim->vehicle_p2 >= 0)
		tarx = parts[sim->vehicle_p2].x, tary = parts[sim->vehicle_p2].y;
}

void Element_CYTK_exit_vehicle(Simulation *sim, Particle *parts, int i, int x, int y) {
	// Flag that vehicle has no stkm
	// Make sure new spawn location isn't too close or STKM
	// will instantly re-enter
	if (parts[i].tmp2 == 1) {
		sim->vehicle_p1 = -1;
		parts[i].tmp2 = 0;
		sim->player.stkmID = sim->create_part(-1, x - MIN_STKM_DISTANCE_TO_ENTER - 5, y - 5, PT_STKM);
		sim->player.elem = parts[i].ctype;
		sim->player.rocketBoots = parts[i].tmp == 1;
	}
	else if (parts[i].tmp2 == 2) {
		sim->vehicle_p2 = -1;
		parts[i].tmp2 = 0;
		sim->player2.stkmID = sim->create_part(-1, x - MIN_STKM_DISTANCE_TO_ENTER - 5, y - 5, PT_STKM2);
		sim->player2.elem = parts[i].ctype;
		sim->player2.rocketBoots = parts[i].tmp == 1;
	}
}

void Element_CYTK_update_vehicle(Simulation *sim, Particle *parts, int i, const Vehicle &v, float ovx, float ovy) {
	// Limit max speed
	if (parts[i].vx > v.max_speed)
		parts[i].vx = v.max_speed;
	else if (parts[i].vx < -v.max_speed)
		parts[i].vx = -v.max_speed;
	if (parts[i].vy > v.max_speed)
		parts[i].vy = v.max_speed;
	else if (parts[i].vy < -v.max_speed)
		parts[i].vy = -v.max_speed;

	// Run over STKM and FIGH if fast enough
	if (fabs(ovx) > v.runover_speed || fabs(ovy) > v.runover_speed) {
		int xdiff = abs(sim->player.legs[0] - parts[i].x);
		int ydiff = abs(sim->player.legs[1] - parts[i].y);
		if (xdiff + ydiff < v.width / 2) {
			sim->kill_part(sim->player.stkmID);
		}

		xdiff = abs(sim->player2.legs[0] - parts[i].x);
		ydiff = abs(sim->player2.legs[1] - parts[i].y);
		if (xdiff + ydiff < v.width / 2) {
			sim->kill_part(sim->player2.stkmID);
		}

		for (unsigned int j = 0; j < MAX_FIGHTERS; ++j) {
			if (!sim->fighters[j].spwn)
				continue;
			xdiff = abs(sim->fighters[j].legs[0] - parts[i].x);
			ydiff = abs(sim->fighters[j].legs[1] - parts[i].y);
			if (xdiff + ydiff < v.width / 2)
				sim->kill_part(sim->fighters[j].stkmID);			
		}
	}

	// Check for nearby STKM and FIGH
	if (sim->vehicle_p1 < 0 && parts[i].tmp2 == 0 && sim->player.spawnID >= 0 && Element_CYTK_check_stkm(&sim->player, sim, parts, i)) {
		parts[i].tmp2 = 1;
		sim->vehicle_p1 = i;
	}
	if (sim->vehicle_p2 < 0 && parts[i].tmp2 == 0 && sim->player2.spawnID >= 0 && Element_CYTK_check_stkm(&sim->player2, sim, parts, i)) {
		parts[i].tmp2 = 2;
		sim->vehicle_p2 = i;
	}
	if (parts[i].tmp2 == 0)
		for (unsigned int j = 0; j < MAX_FIGHTERS; ++j) {
			if (!sim->fighters[j].spwn)
				continue;
			if (Element_CYTK_check_stkm(&sim->fighters[j], sim, parts, i)) {
				parts[i].tmp2 = 3;
				break;
			}
		}
}


// The rest of the stuff below is cybetruck specific
// Anything above is often used in other vehicles too

void Element_CYTK_changeType(ELEMENT_CHANGETYPE_FUNC_ARGS) {
	if (to == PT_NONE && sim->parts[i].life <= 0) {
		// Upon death, turn into a cybertruck shaped pile of BGLA, BRMT and BREC
		int j, t;
		float angle = FROM_FIXED_E(sim->parts[i].tmp3, PT_CYTK);
		for (auto px = CYBERTRUCK_PIXELS.begin(); px != CYBERTRUCK_PIXELS.end(); ++px) {
			t = RNG::Ref().between(0, 100);
			if (t < 20)
				j = Element_CYTK_create_part(sim, px->x, px->y, PT_BGLA, angle, sim->parts, i);
			else if (t < 70)
				j = Element_CYTK_create_part(sim, px->x, px->y, PT_BRMT, angle, sim->parts, i);
			else
				j = Element_CYTK_create_part(sim, px->x, px->y, PT_BREC, angle, sim->parts, i);
			if (j > -1) {
				sim->parts[j].dcolour = 0xAF000000 | PIXRGB(px->r, px->g, px->b);
				sim->parts[j].vx = sim->parts[i].vx;
				sim->parts[j].vy = sim->parts[i].vy;
				sim->parts[j].temp = sim->parts[i].temp;
			}
		}
	}
}

static int update(UPDATE_FUNC_ARGS) {
	// NOTE: Cybertruck UPDATES TWICE PER FRAME
	/**
	 * Properties:
	 * vx, vy (velocity)
	 * ctype = element of STKM when it entered
	 * tmp2 = which STKM controls it (1 = STKM, 2 = STK2, 3 = AI car)
	 * tmp = rocket or flamethrower (0 none, 1 rocket, 2 flamethrower)
	 * temp = damage
	 * life = charge
	 * tmp3 = rotation
	 * tmp4 = direction of travel (left or right)
	 * 
	 * If touched by a FIGH the FIGH will attempt to use the cybertruck to run down
	 * the STKM and STK2
	 */

	// Collision checking and update
	// We'll consider moving in the direction of velocity
	// We only check collisions with 6 points on the rectangle that
	// bounds the cybertruck
	float ovx = parts[i].vx, ovy = parts[i].vy;
	float angle = FROM_FIXED_E(parts[i].tmp3, PT_CYTK);
	bool has_collision;
	Element_CYTK_initial_collision(sim, parts, i, Cybertruck, has_collision);

	// Collision damage
	if (has_collision && (fabs(ovx) > Cybertruck.collision_speed || fabs(ovy) > Cybertruck.collision_speed)) {
		parts[i].life -= RNG::Ref().between(0, 25);
	}

	// Heat damage
	if (parts[i].temp > 273.15f + 200.0f)
		parts[i].life--;
	if (parts[i].temp > 1000.0f)
		parts[i].life = 0;

	// Self destruction, leave a randomized cybertruck shaped pile of powder
	if (parts[i].life <= 0) {
		sim->kill_part(i);
		return 0;
	}

	// If life <= 50 spawn sparks (EMBR)
	if (parts[i].life <= 50) {
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, Cybertruck.width * 0.4f, -Cybertruck.height / 2, PT_EMBR, angle, parts, i);
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, -Cybertruck.width * 0.4f, -Cybertruck.height / 2, PT_EMBR, angle, parts, i);
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, 0, -Cybertruck.height / 2, PT_EMBR, angle, parts, i);
	}
	// If life <= 25 spawn fire damage
	if (parts[i].life <= 25 && RNG::Ref().chance(1, 30)) {
		Element_CYTK_create_part(sim, -Cybertruck.width * 0.4f, -Cybertruck.height / 2, PT_FIRE, angle, parts, i);
	}

	// Player controls
	int cmd = NOCMD, cmd2 = NOCMD;
	if (is_stkm(parts[i].tmp2))
		Element_CYTK_get_player_command(sim, parts, i, cmd, cmd2);
	// Fighter AI
	else if (is_figh(parts[i].tmp2)) {
		int tarx = -1, tary = -1;
		Element_CYTK_get_target(sim, parts, tarx, tary);
		if (tarx > 0) {
			if (parts[i].tmp == 2 || parts[i].tmp == 3) { // Flamethrower / bomb weapon
				cmd2 = DOWN;
				parts[i].tmp4 = tarx > parts[i].x;
			}
			else if (parts[i].tmp == 1) { // Rocket
				cmd = tarx > parts[i].x ? LEFT : RIGHT;
				cmd2 = tary < parts[i].y ? DOWN : NOCMD;
			}
			else if (has_collision) // Run 'em over
				cmd = tarx > parts[i].x ? RIGHT : LEFT;
		}
	}

	// Do controls
	if (cmd != NOCMD || cmd2 != NOCMD) {
		if (has_collision || parts[i].tmp == 1) { // Accelerating only can be done on ground or if rocket
			float ax = has_collision ? -Cybertruck.acceleration : -Cybertruck.fly_acceleration / 8.0f, ay = 0.0f;
			if (cmd == LEFT) { // Left
				rotate(ax, ay, angle);
				parts[i].vx += ax, parts[i].vy += ay;
				parts[i].tmp4 = 0; // Set face direction
				parts[i].y -= 0.5;
			}
			else if (cmd == RIGHT) { // Right
				ax *= -1;
				rotate(ax, ay, angle);
				parts[i].vx += ax, parts[i].vy += ay;
				parts[i].tmp4 = 1; // Set face direction
				parts[i].y -= 0.5;
			}
		}
		if (cmd2 == UP) { // Exit (up)
			Element_CYTK_exit_vehicle(sim, parts, i, x, y);
			return 0;
		}
		else if (cmd2 == DOWN) { // Fly or shoot (down)
			if (parts[i].tmp == 1) { // Rocket
				float ax = 0.0f, ay = -Cybertruck.fly_acceleration / 4.0f;
				rotate(ax, ay, angle);
				parts[i].vx += ax, parts[i].vy += ay;

				int j1 = Element_CYTK_create_part(sim, Cybertruck.width * 0.4f, Cybertruck.height / 2, PT_PLSM, angle, parts, i);
				int j2 = Element_CYTK_create_part(sim, -Cybertruck.width * 0.4f, Cybertruck.height / 2, PT_PLSM, angle, parts, i);
				if (j1 > -1 && j2 > -1) {
					parts[j1].temp = parts[j2].temp = 400.0f;
					parts[j1].life = RNG::Ref().between(0, 100) + 50;
					parts[j2].life = RNG::Ref().between(0, 100) + 50;
				}
			}
			else if (parts[i].tmp == 2) { // Flamethrower
				int j = Element_CYTK_create_part(sim, -Cybertruck.width * 0.4f, -Cybertruck.height / 2, PT_BCOL, angle, parts, i);
				if (j > -1) {
					parts[j].life = RNG::Ref().between(0, 100) + 50;
					parts[j].vx = parts[i].tmp4 ? 15 : -15;
					parts[j].vy = -(RNG::Ref().between(0, 3) + 3);
					rotate(parts[j].vx, parts[j].vy, angle);
				}
			}
			else if (parts[i].tmp == 3 && sim->timer % 50 == 0) { // BOMB
				int j = Element_CYTK_create_part(sim, -Cybertruck.width * 0.4f, -Cybertruck.height / 2, PT_BOMB, angle, parts, i);
				if (j > -1) {
					parts[j].life = RNG::Ref().between(0, 100) + 50;
					parts[j].vx = parts[i].tmp4 ? 10 : -10;
					parts[j].vy = -(RNG::Ref().between(0, 3) + 3);
					rotate(parts[j].vx, parts[j].vy, angle);
				}
			}
		}
	}

	Element_CYTK_update_vehicle(sim, parts, i, Cybertruck, ovx, ovy);
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*cola = 0;
	draw_cybertruck(ren, cpart, FROM_FIXED(cpart->tmp3, ren->sim->elements[PT_CYTK].FixedMultiplier));
	return 0;
}
