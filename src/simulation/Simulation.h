#ifndef SIMULATION_H
#define SIMULATION_H
#include <functional>
#include "Config.h"
#include "simulation/WorldGrid.h"

#include <cstring>
#include <cstddef>
#include <vector>
#include <set>
#include <array>
#include <memory>

#include "Particle.h"
#include "Turbo.h"
#include "Stickman.h"
#include "WallType.h"
#include "Sign.h"
#include "ElementDefs.h"
#include "BuiltinGOL.h"
#include "MenuSection.h"
#include "Stasis.h"
#include "PortalBuffers.h"

#include "CoordStack.h"

#include "Element.h"

#define CHANNELS ((int)(MAX_TEMP-73)/100+2)
#define MAX_FARADAY_DIVISIONS 100
#define MAX_TMP2_CHANNELS 100
#define FARADAY_CHANNELS (MAX_FARADAY_DIVISIONS + MAX_TMP2_CHANNELS)

class Snapshot;
class SimTool;
class Brush;
class SimulationSample;
struct matrix2d;
struct vector2d;

class GameModel;
class Simulation;
class Renderer;
class Gravity;
class Air;
// Class Stasis defined in Stasis.h
class EMField;
class GameSave;
class StressField;

class Simulation
{
public:

	Gravity * grav;
	Air * air;
	EMField * emfield;
	Stasis * stasis;

	std::vector<sign> signs;
	std::array<Element, PT_NUM> elements;
	//Element * elements;
	std::vector<SimTool> tools;
	std::vector<unsigned int> platent;
	std::vector<wall_type> wtypes;
	std::vector<menu_section> msections;

	int currentTick;
	int replaceModeSelected;
	int replaceModeFlags;

	char can_move[PT_NUM][PT_NUM];
	int debug_currentParticle;
	int debug_mostRecentlyUpdated = -1; // -1 when between full update loops
	int parts_lastActiveIndex;
	int pfree;
	int NUM_PARTS;
	bool elementRecount;
	int elementCount[PT_NUM];
	// Particles of each type that appeared (created or changed into) and vanished (killed or changed away)
	// since the page last read them, for its sound effects (tpt_sound_events). Counting only: nothing in the
	// simulation reads them. Unsigned, so a page that never reads them just wraps around.
	unsigned int soundAppeared[PT_NUM] = {};
	// Called at the end of every simulated frame when set (the page's rewind buffer)
	void (*afterSimHook)(Simulation *sim) = nullptr;
	unsigned int soundVanished[PT_NUM] = {};

	// Turbo mode (Turbo.h, Turbo.cpp): threads to update the particles on (0 = off), element types whose
	// changes reach beyond their neighbourhood (never updated, created or removed in parallel), and what the
	// last frame did
	int turboThreads = 0;
	bool turboSpecial[PT_NUM] = {};
	std::vector<unsigned int> turboDone; // turboFrameId for the particles updated in parallel this frame
	unsigned int turboFrameId = 0;
	int turboSerialCount = 0; // particles the last turbo frame updated on one thread
	struct TurboStats
	{
		int frames = 0, tiles = 0, parallelTiles = 0, parallelParticles = 0, serialParticles = 0, deferredMoves = 0;
		int specialTouched = 0; // must stay 0: a special particle was changed in parallel
		double msPrepare = 0, msParallel = 0, msAfter = 0; // summed over the frames
		bool lastFrameTurbo = false;
	} turboStats;
	bool turboElementOk[PT_NUM] = {}; // updated in parallel this frame
	TurboState *turboState = nullptr;
	bool TurboFrame();
	bool TurboRecalc(bool do_life_dec);
	void TurboRows(int rows, const std::function<void(int y0, int y1)> &body);
	TurboState &TurboStateFor();
	bool TurboParticleOk(const Particle &p) const;
	void TurboUpdateList(const int *list, int count);
	void TurboFree();

	// The shared counters a particle update changes; in parallel they are kept per thread and added up after
	void CountElement(int t, int delta)
	{
		if (turboThread)
			turboThread->elementCount[t] += delta;
		else
			elementCount[t] += delta;
	}
	void CountAppeared(int t)
	{
		if (turboThread)
			turboThread->soundAppeared[t]++;
		else
			soundAppeared[t]++;
	}
	void CountVanished(int t)
	{
		if (turboThread)
			turboThread->soundVanished[t]++;
		else
			soundVanished[t]++;
	}
	void NoteActiveParticle(int i)
	{
		if (turboThread)
		{
			if (i > turboThread->lastActive)
				turboThread->lastActive = i;
		}
		else if (i > parts_lastActiveIndex)
			parts_lastActiveIndex = i;
	}
	// A free particle slot, or -1; in parallel, one of the tile's own
	int PopFreeParticle()
	{
		if (turboThread)
		{
			auto *tile = turboThread->tile;
			if (tile->poolNext >= tile->pool.size())
				return -1;
			return tile->pool[tile->poolNext++];
		}
		if (pfree == -1)
			return -1;
		int i = pfree;
		pfree = parts[i].life;
		return i;
	}
	int ISWIRE;
	bool force_stacking_check;
	int emp_decor;
	int emp_trigger_count;
	std::set<int> emp_faraday_channels;
	bool etrd_count_valid;
	int etrd_life0_count;
	int lightningRecreate;
	//Stickman
	playerst player;
	playerst player2;
	playerst fighters[MAX_FIGHTERS]; //Defined in Stickman.h
	unsigned char fighcount; //Contains the number of fighters
	bool gravWallChanged;
	//Vehicles (cybertruck)
	int vehicle_p1 = -1, vehicle_p2 = -1;
	std::vector<int> vehicles;
	const unsigned char MAX_VEHICLES = 100;
	//Portals and Wifi
	PortalBuffers portalp; // indexed by FARADAY_CHANNELS * CHANNELS channels
	int portal_rx[8];
	int portal_ry[8];
	unsigned char wireless[FARADAY_CHANNELS][CHANNELS];
	WorldGrid<unsigned short> faraday_map;
	bool faraday_updated = false;

	//Stress sim
	StressField * stressField;
	//Gol sim
	int CGOL;
	int GSPEED;
	WorldGrid<std::array<unsigned int, 5>> gol;
	//Air sim
	GridView<float> vx;
	GridView<float> vy;
	GridView<float> pv;
	GridView<float> hv;
	//Gravity sim
	float *gravx;//gravx[(YRES/CELL) * (XRES/CELL)];
	float *gravy;//gravy[(YRES/CELL) * (XRES/CELL)];
	float *gravp;//gravp[(YRES/CELL) * (XRES/CELL)];
	float *gravmap;//gravmap[(YRES/CELL) * (XRES/CELL)];
	// Time dilation, smaller = more updates
	WorldGrid<char> time_dilation;
	// false only while every time_dilation cell is 0; lets the per-particle and per-cell checks be skipped
	bool anyTimeDilation = false;
	const char MAX_TIME_DILATION = 8;
	const char MIN_TIME_DILATION = -16;
	//Walls
	WorldGrid<unsigned char> bmap;
	WorldGrid<unsigned char> emap;
	WorldGrid<float> fvx;
	WorldGrid<float> fvy;
	WorldGrid<short> oneWayDir; // 0 = nothing, 1 = up, 2 = left, 3 = down, 4 = right
	//Particles
	std::vector<Particle> partsStorage; // NPART of them; parts points at the first
	Particle *parts;
	WorldGrid<int> pmap;
	WorldGrid<int> photons;
	WorldGrid<unsigned int> pmap_count;
	//Simulation Settings
	int edgeMode;
	int gravityMode;
	float customGravityX;
	float customGravityY;
	int legacy_enable;
	int aheat_enable;
	int water_equal_test;
	int sys_pause;
	int framerender;
	int pretty_powder;
	int sandcolour;
	int sandcolour_frame;
	int deco_space;
	// Timer
	unsigned int timer = 0;

	int Load(const GameSave * save, bool includePressure);
	int Load(const GameSave * save, bool includePressure, int x, int y);
	GameSave * Save(bool includePressure);
	GameSave * Save(bool includePressure, int x1, int y1, int x2, int y2);
	void SaveSimOptions(GameSave * gameSave);
	SimulationSample GetSample(int x, int y);

	std::unique_ptr<Snapshot> CreateSnapshot();
	void Restore(const Snapshot &snap);

	int is_blocking(int t, int x, int y);
	int is_boundary(int pt, int x, int y);
	int find_next_boundary(int pt, int *x, int *y, int dm, int *em, bool reverse);
	void photoelectric_effect(int nx, int ny);
	unsigned direction_to_map(float dx, float dy, int t);
	int do_move(int i, int x, int y, float nxf, float nyf);
	bool move(int i, int x, int y, float nxf, float nyf);
	int try_move(int i, int x, int y, int nx, int ny);
	int eval_move(int pt, int nx, int ny, unsigned *rr);
	void init_can_move();
	bool IsWallBlocking(int x, int y, int type);
	// PIPE/PPIP tmp flag: the pipe formed next to HEAC and conducts heat (heat pipe, as in TPT 100)
	static constexpr int PFLAG_CAN_CONDUCT = 0x00000001;
	static constexpr int PFLAG_PARTICLE_DECO = 0x00000002; // the pipe's deco came from what it carries (upstream e2e25682)
	// Particles that take no part in heat conduction: HeatConduct 0, HSWC while off, and pipes that are
	// not heat pipes
	bool IsHeatInsulator(const Particle &p) const;
	// a pipe with something in it holds the heat of both (upstream e7c708a0)
	float HeatCapacityOf(const Particle &p) const;
	// Challenge mode (Lua challenge.begin): limits on what the player's tools may change. GameController sets
	// userAction while it applies a tool, so Lua scripts and reactions are never limited.
	struct ChallengeRules
	{
		bool active = false;
		int inkBudget = -1; // particles the player may place, -1 = no limit
		int inkUsed = 0;
		std::vector<std::array<int, 4>> locked; // x0, y0, x1, y1 (inclusive) the player's tools cannot change
	} challenge;
	bool userAction = false;
	bool UserMayChange(int x, int y) const;
	bool IsElement(int type) const {
		return (type > 0 && type < PT_NUM && elements[type].Enabled);
	}
	bool IsElementOrNone(int type) const {
		return (type >= 0 && type < PT_NUM && elements[type].Enabled);
	}
	void create_cherenkov_photon(int pp);
	void create_gain_photon(int pp);
	void kill_part(int i, bool ignore_iwall=false);
	bool FloodFillPmapCheck(int x, int y, int type);
	int flood_prop(int x, int y, size_t propoffset, PropertyValue propvalue, StructProperty::PropertyType proptype);
	bool flood_water(int x, int y, int i);
	int FloodINST(int x, int y, int INSTType=-1);
	void RecalculateFaraday();
	void detach(int i);
	bool part_change_type(int i, int x, int y, int t);
	//int InCurrentBrush(int i, int j, int rx, int ry);
	//int get_brush_flags();
	int create_part(int p, int x, int y, int t, int v = -1);
	void delete_part(int x, int y);
	void get_sign_pos(int i, int *x0, int *y0, int *w, int *h);
	int is_wire(int x, int y);
	int is_wire_off(int x, int y);
	void set_emap(int x, int y);
	int parts_avg(int ci, int ni, int t);
	void create_arc(int sx, int sy, int dx, int dy, int midpoints, int variance, int type, int flags);
	void UpdateParticles(int start, int end); // range inclusive on both ends
	template<int Mode>
	void UpdateParticleLoop(int start, int end, const int *list = nullptr, const TurboDeferredMove *moves = nullptr);
	void SimulateGoL();
	void RecalcFreeParticles(bool do_life_dec);
	void CheckStacking();
	void BeforeSim();
	void AfterSim();
	void rotate_area(int area_x, int area_y, int area_w, int area_h, int invert);
	void clear_area(int area_x, int area_y, int area_w, int area_h);

	void SetEdgeMode(int newEdgeMode);
	void SetDecoSpace(int newDecoSpace);

	//Drawing Deco
	void ApplyDecoration(int x, int y, int colR, int colG, int colB, int colA, int mode);
	void ApplyDecorationPoint(int x, int y, int colR, int colG, int colB, int colA, int mode, Brush * cBrush = NULL);
	void ApplyDecorationLine(int x1, int y1, int x2, int y2, int colR, int colG, int colB, int colA, int mode, Brush * cBrush = NULL);
	void ApplyDecorationBox(int x1, int y1, int x2, int y2, int colR, int colG, int colB, int colA, int mode);
	bool ColorCompare(Renderer *ren, int x, int y, int replaceR, int replaceG, int replaceB);
	void ApplyDecorationFill(Renderer *ren, int x, int y, int colR, int colG, int colB, int colA, int replaceR, int replaceG, int replaceB);

	//Drawing Tools like HEAT, AIR, and GRAV
	int Tool(int x, int y, int tool, int brushX, int brushY, float strength = 1.0f);
	int ToolBrush(int x, int y, int tool, Brush * cBrush, float strength = 1.0f);
	void ToolLine(int x1, int y1, int x2, int y2, int tool, Brush * cBrush, float strength = 1.0f);
	void ToolBox(int x1, int y1, int x2, int y2, int tool, float strength = 1.0f);

	//Drawing Walls
	int CreateWalls(int x, int y, int rx, int ry, int wall, Brush * cBrush = NULL);
	void CreateWallLine(int x1, int y1, int x2, int y2, int rx, int ry, int wall, Brush * cBrush = NULL);
	void CreateWallBox(int x1, int y1, int x2, int y2, int wall);
	int FloodWalls(int x, int y, int wall, int bm);

	//Drawing Particles
	int CreateParts(int positionX, int positionY, int c, Brush * cBrush, int flags = -1);
	int CreateParts(int x, int y, int rx, int ry, int c, int flags = -1);
	int CreatePartFlags(int x, int y, int c, int flags);
	void CreateLine(int x1, int y1, int x2, int y2, int c, Brush * cBrush, int flags = -1);
	void CreateLine(int x1, int y1, int x2, int y2, int c);
	void CreateBox(int x1, int y1, int x2, int y2, int c, int flags = -1);
	int FloodParts(int x, int y, int c, int cm, int flags = -1);


	void GetGravityField(int x, int y, float particleGrav, float newtonGrav, float & pGravX, float & pGravY);

	int GetParticleType(ByteString type);

	void orbitalparts_get(int block1, int block2, int resblock1[], int resblock2[]);
	void orbitalparts_set(int *block1, int *block2, int resblock1[], int resblock2[]);
	int get_wavelength_bin(int *wm);
	int get_normal(int pt, int x, int y, float dx, float dy, float *nx, float *ny);
	int get_normal_interp(int pt, float x0, float y0, float dx, float dy, float *nx, float *ny);
	void clear_sim();
	Simulation();
	~Simulation();

	bool InBounds(int x, int y);

	// These don't really belong anywhere at the moment, so go here for loop edge mode
	static int remainder_p(int x, int y);
	static float remainder_p(float x, float y);

	GameModel *model = nullptr;
	Renderer *ren = nullptr;
	GameModel *getModel();

	// Circuits, quantum states and moving solids (TPT Ultimata) live in globals shared by every
	// Simulation; only the one the game runs (set by GameModel) may reset them, not save thumbnails.
	bool ownsGlobalElementState = false;
	void ResetGlobalElementState();

	String ElementResolve(int type, int ctype) const;
	String BasicParticleInfo(Particle const &sample_part) const;


	struct CustomGOLData
	{
		int rule, colour1, colour2;
		String nameString, ruleString;

		inline bool operator <(const CustomGOLData &other) const
		{
			return rule < other.rule;
		}
	};

private:
	std::vector<CustomGOLData> customGol;

public:
	const CustomGOLData *GetCustomGOLByRule(int rule) const;
	const std::vector<CustomGOLData> GetCustomGol() { return customGol; }
	void SetCustomGOL(std::vector<CustomGOLData> newCustomGol);

private:
	CoordStack& getCoordStackSingleton();
};

#endif /* SIMULATION_H */
