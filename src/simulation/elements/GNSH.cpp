#include "simulation/ElementCommon.h"
#include "simulation/vehicles/vehicle.h"
#include "simulation/vehicles/gunship.h"
#include "gui/game/GameModel.h"

#include "simulation/FixedUtils.h"

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

void Element_GNSH_changeType(ELEMENT_CHANGETYPE_FUNC_ARGS);
int  Element_CYTK_create_part(Simulation *sim, int x, int y, int type, float theta, Particle *parts, int i);
void Element_CYTK_get_player_command(Simulation *sim, Particle *parts, int i, int &cmd, int &cmd2);
void Element_CYTK_initial_collision(Simulation *sim, Particle *parts, int i, const Vehicle &v, bool &has_collision);
void Element_CYTK_get_target(Simulation *sim, Particle *parts, int &tarx, int &tary);
void Element_CYTK_exit_vehicle(Simulation *sim, Particle *parts, int i, int x, int y);
void Element_CYTK_update_vehicle(Simulation *sim, Particle *parts, int i, const Vehicle &v, float ovx, float ovy);
void Element_SPDR_intersect_line(Simulation *sim, int sx, int sy, float vx, float vy, int &x, int &y, int type=1, int type2=0);


void Element::Element_GNSH() {
	Identifier = "DEFAULT_PT_GNSH";
	Name = "GNSH";
	Colour = PIXPACK(0x8FA7B3);
	MenuVisible = 1;
	MenuSection = SC_SPECIAL; // SC_CRACKER2 in TPT Ultimata
	Enabled = 1;

	Advection = 0.01f;
	AirDrag = 0.04f * CFDS;
	AirLoss = 0.94f;
	Loss = 0.95f;
	Collision = -0.1f;
	Gravity = 0.07f; // NOTE: GUNSHIP UPDATES TWICE PER FRAME
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 1;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 1;

	Weight = 100;
	HeatConduct = 10;
	Description = "UEF T3 Heavy Gunship. Now rideable! UP = Shoot, DOWN = Fly, L + R + D = exit";

	DefaultProperties.life = 5000; // Default 5000 HP

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
	ChangeType = &Element_GNSH_changeType;
}

static void accelerate_air(float rx, float ry, Simulation *sim, Particle *parts, int i, int x, int y, float t_angle, float speed) {
	rotate(rx, ry, FROM_FIXED_E(parts[i].tmp3, PT_GNSH));
	sim->vx[(int)(y + ry) / CELL][(int)(x + rx) / CELL] += speed * cos(t_angle);
	sim->vy[(int)(y + ry) / CELL][(int)(x + rx) / CELL] += speed * sin(t_angle);

	// Search for nearby birds and kill 'em
	for (int rx2 = -3; rx2 <= 3; ++rx2)
		for (int ry2 = -3; ry2 <= 3; ++ry2)
			if (BOUNDS_CHECK && (rx2 || ry2)) {
				int r = sim->pmap[(int)(y + ry + ry2)][(int)(x + rx + rx2)];
				if (!r || TYP(r) != PT_BIRD) continue;
				sim->part_change_type(ID(r), parts[ID(r)].x, parts[ID(r)].y, PT_BLOD);
			}
}

void Element_GNSH_changeType(ELEMENT_CHANGETYPE_FUNC_ARGS) {
	if (to == PT_NONE && sim->parts[i].life <= 0) {
		// Upon death, turn into a gunship shaped pile of CRBN
		for (auto px = GUNSHIP_BASE.begin(); px != GUNSHIP_BASE.end(); ++px) {
			int j = Element_CYTK_create_part(sim, px->x, px->y, PT_CRBN, FROM_FIXED_E(sim->parts[i].tmp3, PT_GNSH), sim->parts, i);
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
	// NOTE: GNSH UPDATES TWICE PER FRAME
	/**
	 * Properties:
	 * vx, vy (velocity)
	 * ctype = element of STKM when it entered
	 * tmp2 = which STKM controls it (1 = STKM, 2 = STK2, 3 = AI car)
	 * tmp = plasma bolt (0) or laser (1) or missile (2)
	 * life = HP
	 * tmp3 = rotation
	 * tmp4 = direction of travel (left or right)
	 * 
	 * If touched by a FIGH the FIGH will attempt to use the gunship to fire on STKM
	 */
	float ovx = parts[i].vx, ovy = parts[i].vy;
	float angle = FROM_FIXED_E(parts[i].tmp3, PT_GNSH);
	bool has_collision;
	Element_CYTK_initial_collision(sim, parts, i, Gunship, has_collision);

	// Heat damage
	if (parts[i].temp > 273.15f + 9000.0f)
		parts[i].life--;
	// Self destruction, leave a randomized Gunship shaped pile of powder
	if (parts[i].life <= 0) {
		sim->kill_part(i);
		return 0;
	}

	// If life <= 1000 spawn sparks (EMBR)
	if (parts[i].life <= 1000) {
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, Gunship.width * 0.4f, -Gunship.height / 2, PT_EMBR, angle, parts, i);
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, -Gunship.width * 0.4f, -Gunship.height / 2, PT_EMBR, angle, parts, i);
		if (RNG::Ref().chance(1, 50))
			Element_CYTK_create_part(sim, 0, -Gunship.height / 2, PT_EMBR, angle, parts, i);
	}
	// If life <= 300 spawn fire damage
	if (parts[i].life <= 300 && RNG::Ref().chance(1, 30)) {
		Element_CYTK_create_part(sim, -Gunship.width * 0.4f, -Gunship.height / 2, PT_FIRE, angle, parts, i);
	}

	// Player controls
	int cmd = NOCMD, cmd2 = NOCMD;
	int tarx = -1, tary = -1;
	if (is_stkm(parts[i].tmp2))
		Element_CYTK_get_player_command(sim, parts, i, cmd, cmd2);
	// Fighter AI
	else if (is_figh(parts[i].tmp2)) {
		Element_CYTK_get_target(sim, parts, tarx, tary);
		if (tarx > 0) {
			cmd = tarx > parts[i].x ? RIGHT : LEFT;
			cmd2 = RNG::Ref().chance(1, 3) ? UP : NOCMD;
			if (RNG::Ref().chance(1, 10))
				cmd2 = tary < parts[i].y ? UP : DOWN;
		}
	}

	// Do controls
	if (cmd != NOCMD || cmd2 != NOCMD) {
		float ax = -Gunship.acceleration, ay = 0.0f;
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
		else if (cmd == LEFT_AND_RIGHT && cmd2 == DOWN) { // Exit (left and right and down)
			Element_CYTK_exit_vehicle(sim, parts, i, x, y);
			return 0;
		}
		// Thruster angle calculation
		float tangle = (fabs(ax) > 0.2f || fabs(ay) > 0.2f) ? atan2(ay, ax) : PI / 2;
		if (tangle > PI)
			tangle -= PI;

		if (cmd2 == UP) { // Shoot (up)
			// Hover while shooting
			float ax = 0.0f, ay = -0.08f;
			rotate(ax, ay, angle);
			parts[i].vx += ax; parts[i].vy += ay;

			// Accelerate air
			accelerate_air(Gunship.width * 0.4f, Gunship.height / 2, sim, parts, i, x, y, tangle, 7.0f);
			accelerate_air(-Gunship.width * 0.4f, Gunship.height / 2, sim, parts, i, x, y, tangle, 7.0f);

			// Get target, mouse pos or fighter target
			std::pair<float, float> target = parts[i].tmp2 == 3 ?
				std::make_pair((float)tarx, (float)tary) : sim->getModel()->get_mouse_pos();
			float theta = atan2(target.second - parts[i].y, target.first - parts[i].x);

			if (parts[i].tmp == 1) { // Laser
				int tx, ty;
				Element_SPDR_intersect_line(sim, x, y, cos(theta), sin(theta), tx, ty, 9);
				sim->CreateLine(x + 8 * cos(theta), y + 3 * sin(theta), tx, ty, PT_LASR);
			}
			else if (parts[i].tmp != 1 && sim->timer % 50 == 0) { // Guided missile
				int ni = sim->create_part(-1, x + 8 * cos(theta), y + 3 * sin(theta), PT_MSSL);
				if (ni > -1) {
					parts[ni].tmp3 = (int)(target.first + 0.5f);
					parts[ni].tmp4 = (int)(target.second + 0.5f);
					parts[ni].life = 0;
				}
			}
		}
		else if (cmd2 == DOWN) { // Fly (down)
			float ax = 0.0f, ay = -Gunship.fly_acceleration / 4.0f;
			rotate(ax, ay, angle);
			parts[i].vx += ax, parts[i].vy += ay;

			// Accelerate air
			accelerate_air(Gunship.width * 0.4f, Gunship.height / 2, sim, parts, i, x, y, tangle, 7.0f);
			accelerate_air(-Gunship.width * 0.4f, Gunship.height / 2, sim, parts, i, x, y, tangle, 7.0f);

			int j1 = Element_CYTK_create_part(sim, Gunship.width * 0.4f, Gunship.height / 2, PT_SMKE, angle, parts, i);
			int j2 = Element_CYTK_create_part(sim, -Gunship.width * 0.4f, Gunship.height / 2, PT_SMKE, angle, parts, i);
			if (j1 > -1 && j2 > -1) {
				parts[j1].temp = parts[j2].temp = 400.0f;
				parts[j1].life = RNG::Ref().between(0, 100) + 50;
				parts[j2].life = RNG::Ref().between(0, 100) + 50;
			}
		}
	}

	Element_CYTK_update_vehicle(sim, parts, i, Gunship, ovx, ovy);
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	draw_gunship(ren, cpart, cpart->vx, cpart->vy);
	return 0;
}
