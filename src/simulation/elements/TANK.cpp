#include "simulation/ElementCommon.h"
#include "simulation/vehicles/vehicle.h"
#include "simulation/vehicles/kv2.h"

#include "simulation/FixedUtils.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);
static void changeType(ELEMENT_CHANGETYPE_FUNC_ARGS);

int  Element_CYTK_create_part(Simulation *sim, int x, int y, int type, float theta, Particle *parts, int i);
void Element_CYTK_get_player_command(Simulation *sim, Particle *parts, int i, int &cmd, int &cmd2);
void Element_CYTK_initial_collision(Simulation *sim, Particle *parts, int i, const Vehicle &v, bool &has_collision);
void Element_CYTK_get_target(Simulation *sim, Particle *parts, int &tarx, int &tary);
void Element_CYTK_exit_vehicle(Simulation *sim, Particle *parts, int i, int x, int y);
void Element_CYTK_update_vehicle(Simulation *sim, Particle *parts, int i, const Vehicle &v, float ovx, float ovy);

void Element::Element_TANK() {
	Identifier = "DEFAULT_PT_TANK";
	Name = "TANK";
	Colour = PIXPACK(0x52482E);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_CRACKER2 in TPT Ultimata
	Enabled = 1;

	Advection = 0.01f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.15f; // NOTE: TANKS UPDATES TWICE PER FRAME
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;
	Weight = 100;

	HeatConduct = 20;
	Description = "Soviet Tank. STKM can ride, comes with several upgrades. Hold down to shoot.";

	DefaultProperties.life = 600; // Default 600 HP

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
	ChangeType = &changeType;
}

static void changeType(ELEMENT_CHANGETYPE_FUNC_ARGS) {
	if (to == PT_NONE && sim->parts[i].life <= 0) {
		// Die into a tank shaped pile of BRMT and BREC
		int j, t;
		float angle = FROM_FIXED_E(sim->parts[i].tmp3, PT_TANK);
		for (auto px = KV2_PIXELS.begin(); px != KV2_PIXELS.end(); ++px) {
			t = RNG::Ref().between(0, 100);
			if (t < 70)
				j = Element_CYTK_create_part(sim, px->x, px->y, PT_BRMT, angle, sim->parts, i);
			else
				j = Element_CYTK_create_part(sim, px->x, px->y, PT_BREC, angle, sim->parts, i);
			if (j > -1) {
				sim->parts[j].dcolour = 0xCF000000 | PIXRGB(px->r, px->g, px->b);
				sim->parts[j].vx = sim->parts[i].vx;
				sim->parts[j].vy = sim->parts[i].vy;
				sim->parts[j].temp = sim->parts[i].temp;
			}
		}
	}
}

static int update(UPDATE_FUNC_ARGS) {
	// NOTE: TANK UPDATES TWICE PER FRAME
	/**
	 * Properties:
	 * vx, vy (velocity)
	 * ctype = element of STKM when it entered
	 * tmp2 = which STKM controls it (1 = STKM, 2 = STK2, 3 = AI car)
	 * tmp = rocket or flamethrower (0 none, 1 plasma, 2 flamethrower, 3 bomb)
	 * life = HP
	 * tmp3 = rotation
	 * tmp4 = direction of travel (left or right)
	 * 
	 * If touched by a FIGH the FIGH will attempt to use the tank to run down
	 * the STKM and STK2
	 */
	float ovx = parts[i].vx, ovy = parts[i].vy;
	float angle = FROM_FIXED_E(parts[i].tmp3, PT_TANK);
	bool has_collision;
	Element_CYTK_initial_collision(sim, parts, i, KV2, has_collision);

	// Collision damage
	if (has_collision && (fabs(ovx) > KV2.collision_speed || fabs(ovy) > KV2.collision_speed)) {
		parts[i].life -= RNG::Ref().between(0, 25);
	}

	// Heat damage
	if (parts[i].temp > 273.15f + 200.0f)
		parts[i].life--;
	if (parts[i].temp > 3000.0f)
		parts[i].life = 0;

	// Self destruction, leave a randomized KV2 shaped pile of powder
	if (parts[i].life <= 0) {
		sim->kill_part(i);
		return 0;
	}

	// If life <= 150 spawn sparks (EMBR)
	if (parts[i].life <= 150) {
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, KV2.width * 0.4f, -KV2.height / 2, PT_EMBR, angle, parts, i);
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, -KV2.width * 0.4f, -KV2.height / 2, PT_EMBR, angle, parts, i);
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, 0, -KV2.height / 2, PT_EMBR, angle, parts, i);
	}
	// If life <= 300 spawn fire damage
	if (parts[i].life <= 300 && RNG::Ref().chance(1, 30)) {
		Element_CYTK_create_part(sim, -KV2.width * 0.4f, -KV2.height / 2, PT_FIRE, angle, parts, i);
	}

	// Player controls
	int cmd = NOCMD, cmd2 = NOCMD;
	if (is_stkm(parts[i].tmp2))
		Element_CYTK_get_player_command(sim, parts, i, cmd, cmd2);
	// Fighter AI
	else if (is_figh(parts[i].tmp2)) {
		// Get target
		int tarx = -1, tary = -1;
		Element_CYTK_get_target(sim, parts, tarx, tary);
		if (tarx > 0) {
			if (parts[i].tmp == 2 || parts[i].tmp == 3) { // Flamethrower / bomb weapon
				cmd2 = DOWN;
				parts[i].tmp4 = tarx > parts[i].x;
			}
			else if (parts[i].tmp == 1) { // Rocket
				cmd = tarx > parts[i].x ? RIGHT : LEFT;
				cmd2 = tary < parts[i].y ? DOWN : NOCMD;
			}
			else if (has_collision) // Run 'em over
				cmd = tarx > parts[i].x ? RIGHT : LEFT;
		}
	}

	// Do controls
	if (cmd != NOCMD || cmd2 != NOCMD) {
		if (has_collision || parts[i].tmp == 1) { // Accelerating only can be done on ground or if rocket
			float ax = has_collision ? -KV2.acceleration : -KV2.fly_acceleration / 8.0f, ay = 0.0f;
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
			if (parts[i].tmp == 1 || parts[i].tmp == 2) { // Flamethrower
				int j = Element_CYTK_create_part(sim, -KV2.width * 0.4f, -KV2.height / 2, PT_BCOL, angle, parts, i);
				if (j > -1) {
					parts[j].life = RNG::Ref().between(0, 100) + 50;
					parts[j].vx = parts[i].tmp4 ? 15 : -15;
					parts[j].vy = -(RNG::Ref().between(0, 3) + 3);
					if (parts[i].tmp == 1) // Plasma
						parts[j].temp = 9000.0f;
					rotate(parts[j].vx, parts[j].vy, angle);
				}
			}
			else if ((parts[i].tmp == 3 || parts[i].tmp == 0)) { // BOMB
				if (sim->timer % 50 == 0) {
					int j1 = Element_CYTK_create_part(sim, -KV2.width * 0.4f, -KV2.height / 2,
						parts[i].tmp == 3 ? PT_BOMB : PT_FSEP, angle, parts, i);
					if (j1 > -1) {
						parts[j1].life = 20;
						parts[j1].vx = parts[i].tmp4 ? 550 : -550;
						parts[j1].vy = 0;
						parts[j1].temp = 9999.0f;
						rotate(parts[j1].vx, parts[j1].vy, angle);
					}
				}
				// Turret flame when firing (Not with BOMB)
				if (parts[i].tmp != 3) {
					int j2 = Element_CYTK_create_part(sim, -KV2.width * 0.4f + 1, -KV2.height / 2, PT_BANG, angle, parts, i);
					parts[j2].temp = 1500.0f;
				}
			}
		}
	}

	Element_CYTK_update_vehicle(sim, parts, i, KV2, ovx, ovy);
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*cola = 0;
	draw_kv2(ren, cpart, FROM_FIXED(cpart->tmp3, ren->sim->elements[PT_TANK].FixedMultiplier));
	return 0;
}
