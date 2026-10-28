#ifndef AIR_H
#define AIR_H
#include <functional>
#include "Config.h"
#include "WorldGrid.h"

class Simulation;

class Air
{
public:
	Simulation & sim;
	int airMode;
	float ambientAirTemp;
	// vorticity confinement (upstream b77a1cba): > 0 keeps swirls in the air going; 0 is off, the old behaviour
	float vorticityCoeff;
	//Arrays from the simulation
	GridView<unsigned char> bmap;
	GridView<unsigned char> emap;
	GridView<float> fvx;
	GridView<float> fvy;
	//
	WorldGrid<float> vx;
	WorldGrid<float> ovx;
	WorldGrid<float> vy;
	WorldGrid<float> ovy;
	WorldGrid<float> pv;
	WorldGrid<float> opv;
	WorldGrid<float> hv;
	WorldGrid<float> ohv; // Ambient Heat
	WorldGrid<unsigned char> bmap_blockair;
	WorldGrid<unsigned char> bmap_blockairh;
	float kernel[9];
	// runs body(y0, y1) over every band of rows [0, rows), perhaps on several threads at once (turbo mode); each
	// band only reads what no band writes, so the result is the same however it is split
	std::function<void(int rows, const std::function<void(int y0, int y1)> &body)> forRows;
	void make_kernel(void);
	// curl of the air velocity at a cell (dvy/dx - dvx/dy), 0 next to walls (upstream 4d1364b8)
	float vorticity(int y, int x) const;
	void update_airh(void);
	void update_air(void);
	void Clear();
	void ClearAirH();
	void Invert();
	void RecalculateBlockAirMaps();
	Air(Simulation & sim);
};

#endif
