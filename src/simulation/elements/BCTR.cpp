#include "simulation/ElementCommon.h"
#include <iostream>

static int update(UPDATE_FUNC_ARGS);
static int graphics(GRAPHICS_FUNC_ARGS);

#define DIE() {parts[i].tmp2=1;return 0;}
#define EAT(other) {\
	if (other != PT_NONE && RNG::Ref().chance(1, 30))\
		sim->part_change_type(ID(r), parts[ID(r)].x, parts[ID(r)].y, other);\
	else\
		sim->kill_part(ID(r));\
	parts[i].life += BCTR::FOOD_LIFE;\
	return 0;}

namespace BCTR {
	const int TEMP_RES_MULTI = 5;
	const int ENERGY_MULTI = 20;
	const int FOOD_LIFE = 80;
	const int TEMP_LIFE_GAIN = 5; // Energy gained per 0.5 degrees heat
	const int START_LIFE = 110;
	const int AGE_MULTI = 90;
	const int AGE_BASE = 300;
	const int MUTATION_RATE = 2; // Higher = less mutations

	/**
	 * Return an int represented by the bits of the target
	 * from start to end, for example if target was
	 * 000100 and we extracted from 0 to 3, we would get 0001 = 1
	 * 
	 * Assumes 32-bit int
	 */
	int extract_bits(int target, int start, int end) {
		if (target == 0) return 0;
		if (start < 0) start = 0;
		if (end > 31) end = 31;

		unsigned int mask = 0;
		for (int t = start; t <= end; ++t)
			mask |= 1 << t;
		return (target & mask) >> start;
	}

	/**
	 * Apply a random mutation to a target gene
	 */
	int mutate(int gene) {
		// Random flip a bit if mutation allows it
		if (RNG::Ref().chance(1, MUTATION_RATE) != 0)
			return gene;
		return gene ^ (1 << RNG::Ref().between(0, 32));
	}

	void extract_genes(int gene, int &restype, int &resval, int &foodtype, int &metabolism, int &spread,
			int &move, int &graphics, bool &glow) {
		restype = extract_bits(gene, 0, 3);
		resval = extract_bits(gene, 4, 7);
		foodtype = extract_bits(gene, 8, 11);
		metabolism = extract_bits(gene, 12, 15);
		spread = extract_bits(gene, 16, 19);
		move = extract_bits(gene, 20, 22);
		graphics = extract_bits(gene, 23, 25);
		glow = extract_bits(gene, 26, 26);
	}
}

/*
Bacteria features:
- Dies if it runs out of energy/food
- Dies if temperature is too high or low
- Dies if touches SOAP
- Infects STKM

Life stores energy value

tmp stores "age", time to life determined by metabolism: (15 - metabolism) * AGE_MULTI + AGE_BASE

tmp2 is if the bacteria is dead

Genes are stored in ctype
Also see GameView.cpp
4:  0000  - Resistance type (See below)
			1 = HEAT (and FIRE)
			2 = COLD
			3 = VIRS
			4 = SOAP / SALT
			5 = ACID
			6 = Being EATEN
			7 = Radiation
8:  0000  - Resistance value (0 - 15, 0 = no resistance)
12: 0000  - Food type (See below)
			(Not listed) = SUGR
			1 = YEST / DYST
			2 = PLNT / WOOD / SAWD
			3 = PHOT or BRAY
			4 = NEUT / ELEC / PROT
			5 = BCTR
			6 = Thermal energy
16: 0000  - Metabolism speed (0 - 15, 0 = no growth) not including other gene modifiers
	   	    Energy storage is inversely proportional to metabolism
20: 0000  - Spread vector
			(Not listed) - Default liquid
			1 - Stick to solids and powders
			2 - Float through the air
			3 - Randomly swap with liquid particles
			4 - Inject self DNA into other BCTR
23: 000   - Move speed (increased energy cost, 0 - 7)
26: 000   - Graphics (fuzziness?)
			(Not listed) - PMODE_BLUR
			1 - SPARK
			2 - BLOB
			3 - FLAT
27: 0     - Glow in the dark?
31: 00000 - Unused


Will pass on dcolour to children

Genes:
- Resistances:
   - Heat / Cold
   - Radiation
   - VIRS
   - SOAP / ACID
   - Other BCTR
- Metabolize different food sources
   - SUGR (default)
   - YEST (not that far of an evolutionary leap)
   - PLNT / photosynthesis
   - Other BCTR?
- Growth speed
   - How fast it consumes energy
   - Produces gas as it consumes (depends on food)
- Energy storage capacity
   - Spends energy to store however
- Spread vector
   - Stick to solids
   - Spread through air
   - Spread through liquids
   - dissolve into solution. WATR (ctype BCTR)
- Color
   - Fancy colors ooooh
   - Glow in the dark gene, costs energy but is cool

Multiplier for speed of growth
Bacteria requires less sugar to grow as time goes on or if it has enough food
 */

void Element::Element_BCTR() {
	Identifier = "DEFAULT_PT_BCTR";
	Name = "BCTR";
	Colour = PIXPACK(0xDCF781);
	MenuVisible = 1;
	MenuSection = SC_ORGANIC; // SC_ORGANIC in TPT Ultimata; SC_SPECIAL before the menu split
	Enabled = 1;

	Advection = 0.6f;
	AirDrag = 0.01f * CFDS;
	AirLoss = 0.98f;
	Loss = 0.95f;
	Collision = 0.0f;
	Gravity = 0.1f;
	Diffusion = 0.00f;
	HotAir = 0.000f * CFDS;
	Falldown = 2;

	Flammable = 0;
	Explosive = 0;
	Meltable = 0;
	Hardness = 20;

	Weight = 30;
	HeatConduct = 150;
	Description = "Bacteria. Feed it sugar to grow, can evolve genes.";

	Properties = TYPE_LIQUID | PROP_DEADLY | PROP_NEUTPENETRATE;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = 273.15f + 200.0f;
	HighTemperatureTransition = PT_FIRE;

	DefaultProperties.life = BCTR::START_LIFE;

	// Element properties here
	Update = &update;
	Graphics = &graphics;
}

static int update(UPDATE_FUNC_ARGS) {
	// Dead, dissolvable by SOAP
	if (parts[i].tmp2) {
		if (parts[i].temp < 273.15f) {
			parts[i].ctype = PT_BCTR;
			sim->part_change_type(i, parts[i].x, parts[i].y, PT_ICEI);
			return 0;
		}

		int r, rx, ry;
		for (rx = -1; rx < 2; rx++)
		for (ry = -1; ry < 2; ry++)
			if (BOUNDS_CHECK) {
				r = pmap[y + ry][x + rx];
				// Burn
				if (TYP(r) == PT_FIRE || TYP(r) == PT_PLSM) {
					parts[i].life = RNG::Ref().between(0, 200);
					parts[i].temp += 50.0f;
					sim->part_change_type(i, parts[i].x, parts[i].y, PT_FIRE);
					return 0;
				}

				if ((TYP(r) == PT_SALT || TYP(r) == PT_SOAP) && RNG::Ref().chance(1, 100) == 1) {
					sim->kill_part(i);
					return 0;
				}
			}
		return 0;
	}

	// If all genes somehow become 1 become sing :D
	if (parts[i].ctype == ~0) {
		sim->part_change_type(i, parts[i].x, parts[i].y, PT_SING);
		return 0;
	}

	int restype, resval, foodtype, metabolism, spread, move, graphics; bool glow;
	BCTR::extract_genes(parts[i].ctype, restype, resval, foodtype, metabolism, spread, move, graphics, glow);

	// std::cout << restype << " res val" << resval << " food: " << foodtype << " " << metabolism << " spread: " <<
	//	spread << " move: " << move << " graphics:" << graphics << " glow:" << glow << "\n";

	// Temp line
	metabolism = 7;

	int energy_capcity = 2 * BCTR::START_LIFE + (15 - metabolism) * BCTR::ENERGY_MULTI;

	// Inc age, randomize slightly
	if (RNG::Ref().chance(1, 20) != 1)
		parts[i].tmp++;

	// Utilize energy
	if (sim->timer % 50 == 0) {
		int energy_consumption = metabolism + move + glow + 1; // Can't have 0 energy consumption
		parts[i].life -= energy_consumption;
	}

	// Die if no food
	if (parts[i].life <= 0) {
		DIE()
		parts[i].life = 0;
	}
	// Too old
	if (parts[i].tmp >= (15 - metabolism) * BCTR::AGE_MULTI + BCTR::AGE_BASE) {
		// Replace self with new cell if enough energy
		if (parts[i].life >= 2 * BCTR::START_LIFE) {
			parts[i].ctype = BCTR::mutate(parts[i].ctype);
			parts[i].tmp = 0;
			parts[i].life -= BCTR::START_LIFE;
			return 0;
		} else DIE()
	}

	// Temperature
	int max_temp = 273.15f + 70.0f + BCTR::TEMP_RES_MULTI * (restype == 1) * resval;
	if (parts[i].temp > max_temp)
		DIE()
	int min_temp = 273.15f - BCTR::TEMP_RES_MULTI * (restype == 2) * resval;
	if (parts[i].temp < min_temp) {
		parts[i].ctype = PT_BCTR;
		sim->part_change_type(i, parts[i].x, parts[i].y, PT_ICEI);
		DIE()
	}

	int r, rx, ry, rt;
	for (rx = -1; rx < 2; rx++) {
		for (ry = -1; ry < 2; ry++) {
			if (BOUNDS_CHECK && (rx || ry)) {
				r = pmap[y + ry][x + rx];
				rt = TYP(r);

				// Burn
				if ((rt == PT_FIRE || rt == PT_PLSM) && RNG::Ref().chance(1, 80) && (restype != 1 || resval < 8)) {
					parts[i].life = RNG::Ref().between(0, 100);
					parts[i].temp += 30.0f;
					sim->part_change_type(i, parts[i].x, parts[i].y, PT_FIRE);
					return 0;
				}

				// Eat other bacteria
				if (foodtype == 5 && TYP(r) == PT_BCTR && RNG::Ref().chance(1, 200) == 1) {
					int restype2, resval2, foodtype2, metabolism2, spread2, move2, graphics2; bool glow2;
					BCTR::extract_genes(parts[ID(r)].ctype, restype2, resval2, foodtype2, metabolism2, spread2, move2, graphics2, glow2);
					
					if (restype2 != 6 || (restype2 == 6 && RNG::Ref().between(0, 15) > resval2)) {
						// Glow gene is always passed
						if (glow2) parts[ID(i)].ctype |= 1 << 26;
						EAT(PT_NONE)
					}
				}

				// Movement
				// 1 = stick to solids and powders
				if (r && move == 1 && (sim->elements[rt].Properties & TYPE_SOLID || sim->elements[rt].Properties & TYPE_PART)) {
					parts[i].vx = 0;
					parts[i].vy = 0;
				}
				// 2 = Float through the air
				else if (!r && move == 2 && RNG::Ref().chance(move, 20)) {
					parts[i].vx += rx;
					parts[i].vy += ry;
				}
				// 3 = Swap with liquid particles
				else if (r && move == 3 && sim->elements[rt].Properties & TYPE_LIQUID && RNG::Ref().chance(move, 20)) {	
					parts[i].x = parts[ID(r)].x;
					parts[i].y = parts[ID(r)].y;
					parts[ID(r)].x = x;
					parts[ID(r)].y = y;
					pmap[y][x] = r;
					pmap[y + ry][x + rx] = PMAP(i, parts[i].type);
					x = x + rx, y = y + ry;
					return 0;
				}
				// 4 = Reproduce by injecting DNA into other BCTR
				else if (r && move == 4 && TYP(r) == PT_BCTR && parts[i].life > BCTR::START_LIFE && RNG::Ref().chance(1, 100)) {
					parts[i].ctype = BCTR::mutate(parts[i].ctype);
					parts[ID(r)].ctype = parts[i].ctype;
					parts[i].life -= BCTR::START_LIFE;
					return 0;
				}

				if ((!r || TYP(r) == PT_BCTR) && move != 4) {
					// Reproduce if enough energy stored and spot is empty
					if (parts[i].life >= 2 * BCTR::START_LIFE) {
						parts[i].life -= BCTR::START_LIFE;
						int ni = r > 0 ? ID(r) : sim->create_part(-1, x + rx, y + ry, PT_BCTR);
						if (ni > -1) {
							parts[ni].dcolour = parts[i].dcolour;
							// Mutate new bacteria
							parts[ni].ctype = BCTR::mutate(parts[i].ctype);
						}
					}
					r = sim->photons[y + ry][x + rx];
				}
				if (!r) continue;
				rt = TYP(r);

				// Consume food, boost energy
				// Metabolism is % chance of eating / 5
				if (parts[i].life < energy_capcity && RNG::Ref().between(0, 500) < metabolism) {
					if ((foodtype == 1 && (rt == PT_YEST || rt == PT_DYST)) ||
						(foodtype == 2 && (rt == PT_WOOD || rt == PT_PLNT || rt == PT_SAWD || 
							((rt == PT_FLSH || rt == PT_POTO || rt == PT_STMH || rt == PT_UDDR) && RNG::Ref().chance(1, 50))
						)) ||
						(foodtype == 4 && (rt == PT_NEUT || rt == PT_PROT))
					)
						EAT(PT_GAS)
					else if (foodtype == 3 && (rt == PT_PHOT || rt == PT_BRAY)) {
						parts[i].temp += 2.0f;
						EAT(PT_NONE)
					}
					else if (foodtype == 6 && r && parts[i].temp < parts[ID(r)].temp) { // Absorb thermal energy
						parts[i].temp += 0.5f;
						parts[ID(r)].temp -= 0.5f;
						parts[i].life += BCTR::TEMP_LIFE_GAIN;
					}
					else if ((foodtype < 1 || foodtype > 6) && (TYP(r) == PT_SWTR || TYP(r) == PT_SUGR))
						EAT(PT_GAS)
				}

				// Determine resistance against other elements
				// Active resistances
				bool should_res = RNG::Ref().between(0, 15) <= resval;
				if ((rt == PT_VRSS || rt == PT_VRSG || rt == PT_VIRS) && restype == 3 && should_res) // VIRS
					parts[ID(r)].tmp3 = 1;
				else if (rt == PT_ACID && restype == 5 && should_res) { // ACID
					sim->kill_part(ID(r));
					return 0;
				}

				// Passive resistances
				else if ((rt == PT_SALT || rt == PT_SOAP) && RNG::Ref().chance(1, 10) && (restype != 4 || (restype == 4 && !should_res)))
					DIE()

				// Radiation causes mutations, resistance decreases change
				else if ((rt == PT_PROT || rt == PT_NEUT || rt == PT_ELEC) &&
						RNG::Ref().chance(1, 10) && (restype != 7 || (restype == 7 && !should_res)))
					parts[i].ctype = BCTR::mutate(parts[i].ctype);

				// Dies when touching LEAD or BRAS or SUFR
				else if (rt == PT_LEAD || rt == PT_BRAS || rt == PT_SUFR || rt == PT_BRNZ)
					DIE()
			}
		}
	}
	return 0;
}

static int graphics(GRAPHICS_FUNC_ARGS) {
	*pixel_mode |= PMODE_BLUR;

	// Dead
	if (cpart->tmp2) {
		*colr *= 0.8; *colg *= 0.8; *colb *= 0.8;
		*colr += RNG::Ref().between(-10, 10);
		*colg += RNG::Ref().between(-10, 10);
		*colb += RNG::Ref().between(-10, 10);
		return 0;
	}

	// Color vary a bit depending on mutation
	*colr = (*colr + (cpart->ctype ^ 9999) % 255) / 2;
	*colg = (*colg + 255 - *colr) / 2;
	*colb = (*colb + 255 - (cpart->ctype ^ 999999) % 255) / 2;

	// Graphical effectss
	int graphics = BCTR::extract_bits(cpart->ctype, 23, 25);
	if (graphics == 1) *pixel_mode = PMODE_SPARK;
	else if (graphics == 2) *pixel_mode = PMODE_BLOB;
	else if (graphics == 3) *pixel_mode = PMODE_FLAT;

	// Glow in the dark
	int glow = BCTR::extract_bits(cpart->ctype, 26, 26);
	if (glow) {
		*firer = *colr;
		*fireg = *colg;
		*fireb = *colb;
		*firea = 240;
		*pixel_mode |= PMODE_GLOW;
	}

	return 0;
}
