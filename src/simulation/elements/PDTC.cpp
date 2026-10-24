#include "simulation/ElementCommon.h"

static int update(UPDATE_FUNC_ARGS);

void Element::Element_PDTC() {
	Identifier = "DEFAULT_PT_PDTC";
	Name = "PDTC";
	Colour = PIXPACK(0xFDA118);
	MenuVisible = 1;
	MenuSection = SC_SENSOR;
	Enabled = 1;

	Advection = 0.0f;
	AirDrag = 0.00f * CFDS;
	AirLoss = 0.96f;
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

	HeatConduct = 0;
	Description = "Photon Detector, outputs to WIFI channels depending on wavelength, floodfills nearby FILT";

	Properties = TYPE_SOLID;

	LowPressure = IPL;
	LowPressureTransition = NT;
	HighPressure = IPH;
	HighPressureTransition = NT;
	LowTemperature = ITL;
	LowTemperatureTransition = NT;
	HighTemperature = ITH;
	HighTemperatureTransition = NT;

	DefaultProperties.temp = 273.15f + 1.0f;

	Update = &update;
}

static int update(UPDATE_FUNC_ARGS) {
	/**
	 * Props:
	 * tmp:
	 * 0: Default behavior
	 * 1: Don't output to WIFI
	 * 2: Don't output to FILT
	 * 
	 * tmp2:
	 * 0: Kill photon
	 * 1: Don't kill photon
	 * 
	 * temp: delay before next measurement (in frames in C, < 0 = 0)
	 * life: delay counter
	 */
	if (parts[i].life > 0) {
		parts[i].life--;
		return 0;
	}

	int rx, ry, rt, r;
	for (rx = -2; rx < 3; rx++)
		for (ry = -2; ry < 3; ry++)
			if (BOUNDS_CHECK && (rx || ry)) {
				r = sim->photons[y + ry][x + rx];
				if (!r) continue;
				rt = TYP(r);

				if (rt == PT_PHOT) {
					parts[i].life = parts[i].temp - 273.15f;

					bool output_wifi = parts[i].tmp != 1;
					bool output_filt = parts[i].tmp != 2;

					if (output_wifi) {
						int channel = parts[ID(r)].ctype % 101 + 1;
						sim->wireless[sim->faraday_map[y / CELL][x / CELL]][channel] |= 1;
						sim->ISWIRE = 2;
					}

					if (output_filt) {
						int r2;
						for (rx = -1; rx < 2; rx++) {
						for (ry = -1; ry < 2; ry++) {
							if (BOUNDS_CHECK && (rx || ry)) {
								r2 = pmap[y + ry][x + rx];
								if (r2 && TYP(r2) == PT_FILT) {
									PropertyValue value;
									value.Integer = parts[ID(r)].ctype;
									sim->flood_prop(parts[ID(r2)].x, parts[ID(r2)].y, offsetof(Particle, ctype), value, StructProperty::Integer);
									break;
								}
							}
						}}
					}

					// Kill photon if allowed
					if (parts[i].tmp2 == 0)
						sim->kill_part(ID(r));
					return 0;
				}
			}

	return 0;
}
