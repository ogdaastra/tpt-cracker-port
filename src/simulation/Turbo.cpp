// Turbo mode: the particle update on several threads. How it works and why it is safe: Turbo.h.
#include "Simulation.h"
#include "Turbo.h"
#include "ElementClasses.h"
#include "Gravity.h"
#include "magnetics/magnetics.h"
#include "stress/stress.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#if defined(TPT_WEB_THREADS) || !defined(__EMSCRIPTEN__)
# define TURBO_THREADS 1
# include <pthread.h>
#endif

thread_local TurboThreadState *turboThread = nullptr;

namespace
{
// Elements whose update was read and found to act only within TURBO_ELEMENT_REACH pixels, without shared state
// beyond what Turbo.h routes per thread, and to turn things only into elements without side effects elsewhere.
// Elements without an update function are checked by the same rules: their transitions are checked each frame
// (turboElementOk). Add an element here only after reading its update function and everything it calls.
const char *const turboAllowed[] = {
	// no update function: the core loop only (heat, transitions, movement)
	"DEFAULT_PT_SAND", "DEFAULT_PT_DUST", "DEFAULT_PT_STNE", "DEFAULT_PT_SALT", "DEFAULT_PT_SAWD",
	"DEFAULT_PT_CNCT", "DEFAULT_PT_OIL", "DEFAULT_PT_GAS", "DEFAULT_PT_SMKE", "DEFAULT_PT_BRCK",
	"DEFAULT_PT_DMND", "DEFAULT_PT_INSL", "DEFAULT_PT_METL", "DEFAULT_PT_MWAX", "DEFAULT_PT_WAX",
	"DEFAULT_PT_LNTG", "DEFAULT_PT_LO2", "DEFAULT_PT_PSCN", "DEFAULT_PT_NSCN", "DEFAULT_PT_ROCK",
	"DEFAULT_PT_CFLM",
	// update read: neighbours within 1 pixel; become SLTW, FIRE (ctype WATR), SAND, STNE; remove FIRE, PLNT
	"DEFAULT_PT_WATR", "DEFAULT_PT_SLTW", "DEFAULT_PT_DSTW", "DEFAULT_PT_WTRV",
	// Element_FIRE_update (and its legacy half, and Element_THOR_update for molten THOR): neighbours within 2;
	// become or make NBLE, DSTW, SMKE, LAVA, BREC, FIRE, STNE, WATR, RADN, PLUT, NEUT
	"DEFAULT_PT_FIRE", "DEFAULT_PT_PLSM", "DEFAULT_PT_LAVA",
	// within 1 (SALT, SLTW, FRZZ neighbours); become SLTW, ICEI, LQUD (ctype rule in TurboParticleOk)
	"DEFAULT_PT_ICEI", "DEFAULT_PT_SNOW",
	// itself only (BCOL); within 1 (FIRE); within 2 (PLNT: PLNT, VINE, O2, FIRE); within 1 (CO2: WATR, CBNW,
	// O2, NEUT, ELEC; NBLE: CO2, NEUT, ELEC, PHOT, PLSM)
	"DEFAULT_PT_WOOD", "DEFAULT_PT_COAL", "DEFAULT_PT_BCOL", "DEFAULT_PT_PLNT", "DEFAULT_PT_CO2", "DEFAULT_PT_NBLE",
};

// How far a particle updated in parallel can reach from where it started: its move (at most TURBO_MAX_MOVE,
// longer ones are put off), then the liquid search (30 sideways, then 30 along gravity, + 1); air cells within
// reach of what its element update touches.
constexpr int TURBO_LIQUID_SEARCH = 31;
constexpr int TURBO_ELEMENT_REACH = 2;
constexpr int TURBO_REACH_STANDARD = int(TURBO_MAX_MOVE) + TURBO_LIQUID_SEARCH + 1; // sideways, then down
constexpr int TURBO_REACH_CUSTOM = int(TURBO_MAX_MOVE) + 2 * TURBO_LIQUID_SEARCH + 1; // across, then along a uniform pull
constexpr int TURBO_REACH_NO_LIQUIDS = int(TURBO_MAX_MOVE) + TURBO_ELEMENT_REACH + 2 * CELL + 1;

constexpr int TURBO_MAX_THREADS = 8;
constexpr int TURBO_MIN_PARTICLES = 5000;
// The particles are sorted into tiles in this many pieces of the particle list, whatever the number of threads
constexpr int TURBO_CHUNKS = 8;
// Tiles are coloured 3 x 3: two tiles of the same colour have two tiles between them
constexpr int TURBO_COLOURS = 3;
// TurboRecalc fills the particle maps in this many bands of rows
constexpr int TURBO_BANDS = 16;
// How long a thread waits for the next phase by checking before it sleeps
constexpr int TURBO_SPIN = 200000;

double TurboNow()
{
	return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

struct TurboTileSet
{
	TurboTile tile;
	std::vector<int> pieces[TURBO_CHUNKS]; // its particles when the frame started, by piece of the particle list
	size_t count = 0;
};

// Threads that wait for a job, then take parts of it until none are left; the main thread takes parts too.
// Threads are made when first needed and then kept: in a browser a new thread only starts once the main
// thread returns to the browser, so waiting for one to finish could wait forever.
struct TurboPool
{
	TurboState *state = nullptr;
	std::mutex mutex;
	std::condition_variable wake;
	// generation << 32 | parts in it, published together so that a thread always knows how many parts the
	// generation it saw has (a count read separately could already belong to the next one)
	std::atomic<uint64_t> published{ 0 };
	std::atomic<int> active{ 1 }; // threads taking part, the main thread included
	bool quit = false; // under mutex
	void (*job)(TurboState &st, int worker, int part) = nullptr; // set before its generation is published
	std::atomic<uint64_t> next{ 0 }; // generation << 32 | next part
	std::atomic<int> remaining{ 0 };
	int made = 1; // threads made, the main thread included
	void Ensure(int count);
	void Run(void (*job)(TurboState &, int, int), int parts);
	void Work(int worker, uint32_t gen, uint32_t parts);
	static void *ThreadMain(void *arg);
};

struct TurboState
{
	Simulation *sim;
	int reach = 0, tileSize = 0, tilesX = 0, tilesY = 0;
	std::vector<TurboTileSet> tiles;
	std::vector<unsigned char> specialCells; // cells with a particle or wall that must not be touched in parallel
	std::vector<int> specialSum; // its 2-D prefix sums
	std::vector<int> chunkSpecial[TURBO_CHUNKS]; // special cells found by each piece
	std::vector<int> tileOfX, tileOfY; // tile column of each x, first tile of the row of each y (no division per particle)
	std::vector<int> bandOfY; // TurboRecalc's band of each row
	std::vector<TurboThreadState> threads; // [0] is the main thread
	std::vector<TurboTileSet *> phase;
	int lastActive = 0;
	TurboPool pool;
	// TurboRecalc, per piece of the particle list: its particles on the maps by band of rows, in index order
	// (~i for one whose life ran out, removed by RecalcFreeParticles as it goes), what it counted and its free slots
	bool lifeDec = false;
	// TurboRows
	int rows = 0;
	const std::function<void(int, int)> *rowsBody = nullptr;
	struct Piece
	{
		std::vector<int> bands[TURBO_BANDS];
		std::vector<int> kills;
		std::vector<int> elementCount;
		int count = 0, lastUsed = -1, freeHead = -1, freeTail = -1;
	} pieces[TURBO_CHUNKS];
};

namespace
{
struct WorkerArg
{
	TurboPool *pool;
	int worker;
};

// Job: sort one piece of the particle list into tiles
void SortPiece(TurboState &st, int, int chunk)
{
	auto *sim = st.sim;
	int n = st.lastActive + 1;
	int i0 = int(int64_t(n) * chunk / TURBO_CHUNKS), i1 = int(int64_t(n) * (chunk + 1) / TURBO_CHUNKS);
	const int cw = XRES / CELL;
	auto &special = st.chunkSpecial[chunk];
	special.clear();
	for (int i = i0; i < i1; i++)
	{
		auto &p = sim->parts[i];
		int t = p.type;
		if (!t)
			continue;
		int x = std::clamp((int)(p.x + 0.5f), 0, XRES - 1);
		int y = std::clamp((int)(p.y + 0.5f), 0, YRES - 1);
		if (sim->turboSpecial[t])
			special.push_back((y / CELL) * cw + x / CELL);
		else if (sim->turboElementOk[t])
			st.tiles[st.tileOfY[y] + st.tileOfX[x]].pieces[chunk].push_back(i);
	}
}

// Job (TurboRecalc): one piece of the particle list: where each particle is, counting, life running down, free
// slots; the particles whose life ran out are noted, removed afterwards in index order
void RecalcPiece(TurboState &st, int, int chunk)
{
	auto *sim = st.sim;
	int n = st.lastActive + 1;
	int i0 = int(int64_t(n) * chunk / TURBO_CHUNKS), i1 = int(int64_t(n) * (chunk + 1) / TURBO_CHUNKS);
	auto &piece = st.pieces[chunk];
	for (auto &band : piece.bands)
		band.clear();
	piece.kills.clear();
	piece.count = 0;
	piece.lastUsed = piece.freeHead = piece.freeTail = -1;
	bool recount = sim->elementRecount;
	if (recount)
		piece.elementCount.assign(PT_NUM, 0);
	bool lifeDec = st.lifeDec && (!sim->sys_pause || sim->framerender);
	auto &elements = sim->elements;
	for (int i = i0; i < i1; i++)
	{
		auto &p = sim->parts[i];
		int t = p.type;
		if (!t)
		{
			if (piece.freeTail < 0)
				piece.freeHead = i;
			else
				sim->parts[piece.freeTail].life = i;
			piece.freeTail = i;
			continue;
		}
		int x = (int)(p.x + 0.5f), y = (int)(p.y + 0.5f);
		bool inBounds = x >= 0 && y >= 0 && x < XRES && y < YRES;
		piece.lastUsed = i;
		piece.count++;
		if (recount && t >= 0 && t < PT_NUM && elements[t].Enabled)
			piece.elementCount[t]++;
		// the same as RecalcFreeParticles from here
		bool dies = false;
		if (!(sim->anyTimeDilation && inBounds && !(elements[t].Properties & PROP_NO_TIME) && sim->time_dilation[y / CELL][x / CELL] < 0 &&
				sim->timer % (int)abs(sim->time_dilation[y / CELL][x / CELL]) != 0) && lifeDec)
		{
			if (t < 0 || t >= PT_NUM || !elements[t].Enabled)
				dies = true;
			else
			{
				unsigned int props = elements[t].Properties;
				bool stasis = inBounds && sim->bmap[y / CELL][x / CELL] == WL_STASIS && sim->emap[y / CELL][x / CELL] < 8;
				if (p.life > 0 && (props & PROP_LIFE_DEC) && !stasis)
				{
					if (sim->anyTimeDilation && inBounds && !(props & PROP_NO_TIME) && sim->time_dilation[y / CELL][x / CELL] > 0)
					{
						p.life -= sim->time_dilation[y / CELL][x / CELL];
						if (p.life < 1)
							p.life = 1;
					}
					p.life--;
					dies = p.life <= 0 && (props & (PROP_LIFE_KILL_DEC | PROP_LIFE_KILL));
				}
				else
					dies = p.life <= 0 && (props & PROP_LIFE_KILL) && !stasis;
			}
			// kill_part leaves particles in indestructible walls
			if (dies && inBounds && sim->bmap[y / CELL][x / CELL] == WL_INDESTRUCTIBLE)
				dies = false;
			if (dies)
				piece.kills.push_back(i);
		}
		if (inBounds)
			piece.bands[st.bandOfY[y]].push_back(dies ? ~i : i);
	}
}

// Job (TurboRecalc): one band of rows of the particle maps, from the particles placed in it, in index order, as
// RecalcFreeParticles fills them (a particle whose life ran out is placed, then taken off by kill_part)
void PlaceBand(TurboState &st, int, int band)
{
	auto *sim = st.sim;
	int y0 = YRES * band / TURBO_BANDS, y1 = YRES * (band + 1) / TURBO_BANDS;
	int from = y0 * XRES, to = y1 * XRES;
	int *pmap = sim->pmap.Data(), *photons = sim->photons.Data();
	unsigned int *count = sim->pmap_count.Data();
	std::fill(pmap + from, pmap + to, 0);
	std::fill(photons + from, photons + to, 0);
	std::fill(count + from, count + to, 0u);
	auto &elements = sim->elements;
	for (auto &piece : st.pieces)
		for (int v : piece.bands[band])
		{
			bool dies = v < 0;
			int i = dies ? ~v : v;
			auto &p = sim->parts[i];
			int t = p.type;
			int at = (int)(p.y + 0.5f) * XRES + (int)(p.x + 0.5f);
			if (elements[t].Properties & TYPE_ENERGY)
				photons[at] = dies ? 0 : PMAP(i, t);
			else
			{
				if (!pmap[at] || (t != PT_INVIS && t != PT_FILT && t != PT_PINV && t != PT_PFLT))
					pmap[at] = dies ? 0 : PMAP(i, t);
				if (t != PT_THDR && t != PT_EMBR && t != PT_FIGH && t != PT_PLSM && t != PT_FILL &&
					t != PT_SPDR && t != PT_BIRD && t != PT_ANT && t != PT_BEE && t != PT_FISH && t != PT_HAIR)
					count[at]++;
			}
		}
}

// Job (TurboRows): one band of rows
void RowBand(TurboState &st, int, int band)
{
	int y0 = st.rows * band / TURBO_BANDS, y1 = st.rows * (band + 1) / TURBO_BANDS;
	if (y0 < y1)
		(*st.rowsBody)(y0, y1);
}

// Job: update one tile of the current phase
void UpdateTile(TurboState &st, int worker, int part)
{
	auto *sim = st.sim;
	auto &set = *st.phase[part];
	auto &tile = set.tile;
	auto &ts = st.threads[worker];
	// the tile's particles that are still in it and are of elements updated in parallel
	tile.list.clear();
	for (auto &piece : set.pieces)
		for (int i : piece)
		{
			auto &p = sim->parts[i];
			int x = (int)(p.x + 0.5f), y = (int)(p.y + 0.5f);
			// a particle in a faster time field updates several times in a frame, reaching further than one
			// update does: those go on one thread
			if (p.type && x >= tile.x0 && x < tile.x1 && y >= tile.y0 && y < tile.y1 && sim->TurboParticleOk(p) &&
				!(sim->anyTimeDilation && sim->time_dilation[y / CELL][x / CELL] > 0))
				tile.list.push_back(i);
		}
	if (tile.list.empty())
		return;
	ts.tile = &tile;
	turboThread = &ts;
	tptThreadRNG = &tile.rng;
	sim->TurboUpdateList(tile.list.data(), int(tile.list.size()));
	tptThreadRNG = nullptr;
	turboThread = nullptr;
	ts.tile = nullptr;
}
}

void TurboPool::Work(int worker, uint32_t gen, uint32_t parts)
{
	while (true)
	{
		// a part claimed here belongs to generation gen, which therefore cannot end (nor its job change)
		// until the part is done
		uint64_t v = next.load(std::memory_order_acquire);
		bool claimed = false;
		while (uint32_t(v >> 32) == gen && uint32_t(v) < parts)
		{
			if (next.compare_exchange_weak(v, v + 1, std::memory_order_acq_rel))
			{
				claimed = true;
				break;
			}
		}
		if (!claimed)
			return;
		job(*state, worker, int(uint32_t(v)));
		remaining.fetch_sub(1, std::memory_order_acq_rel);
	}
}

void *TurboPool::ThreadMain(void *arg)
{
	auto *a = static_cast<WorkerArg *>(arg);
	auto *pool = a->pool;
	int worker = a->worker;
	delete a;
	uint32_t seen = 0;
	while (true)
	{
		// the next phase usually comes right away; between frames, sleep
		uint64_t pub = pool->published.load(std::memory_order_acquire);
		for (int spin = 0; uint32_t(pub >> 32) == seen && spin < TURBO_SPIN; spin++)
			pub = pool->published.load(std::memory_order_acquire);
		if (uint32_t(pub >> 32) == seen || worker >= pool->active.load(std::memory_order_acquire))
		{
			std::unique_lock<std::mutex> lock(pool->mutex);
			pool->wake.wait(lock, [&] {
				return pool->quit || (uint32_t(pool->published.load() >> 32) != seen && worker < pool->active.load());
			});
			if (pool->quit)
				return nullptr;
			pub = pool->published.load(std::memory_order_acquire);
		}
		seen = uint32_t(pub >> 32);
		pool->Work(worker, seen, uint32_t(pub));
	}
}

void TurboPool::Ensure(int count)
{
	active.store(count, std::memory_order_release);
#ifdef TURBO_THREADS
	for (; made < count; made++)
	{
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 1 << 20);
		pthread_t thread;
		if (pthread_create(&thread, &attr, ThreadMain, new WorkerArg{ this, made }) == 0)
			pthread_detach(thread);
		pthread_attr_destroy(&attr);
	}
#endif
}

void TurboPool::Run(void (*newJob)(TurboState &, int, int), int newParts)
{
	if (newParts <= 0)
		return;
	uint32_t gen;
	{
		std::lock_guard<std::mutex> lock(mutex);
		job = newJob;
		remaining.store(newParts, std::memory_order_relaxed);
		gen = uint32_t(published.load(std::memory_order_relaxed) >> 32) + 1;
		next.store(uint64_t(gen) << 32, std::memory_order_release);
		published.store(uint64_t(gen) << 32 | uint32_t(newParts), std::memory_order_release);
	}
	if (active.load(std::memory_order_relaxed) > 1)
		wake.notify_all();
	Work(0, gen, uint32_t(newParts));
	// threads still on their last part
	while (remaining.load(std::memory_order_acquire) > 0)
		;
}

TurboState &Simulation::TurboStateFor()
{
	if (!turboState)
	{
		turboState = new TurboState();
		turboState->sim = this;
		turboState->pool.state = turboState;
		turboState->threads.resize(TURBO_MAX_THREADS);
	}
	turboState->pool.Ensure(std::clamp(turboThreads, 1, TURBO_MAX_THREADS));
	return *turboState;
}

// RecalcFreeParticles on all threads (turbo mode on more than one): the particle maps, counts, life running down
// and the free list, exactly as RecalcFreeParticles makes them (so either can be used)
bool Simulation::TurboRecalc(bool do_life_dec)
{
	auto &st = TurboStateFor();
	st.lastActive = parts_lastActiveIndex;
	st.lifeDec = do_life_dec;
	if (int(st.bandOfY.size()) != YRES)
	{
		st.bandOfY.resize(YRES);
		for (int band = 0; band < TURBO_BANDS; band++)
			for (int y = YRES * band / TURBO_BANDS; y < YRES * (band + 1) / TURBO_BANDS; y++)
				st.bandOfY[y] = band;
	}
	st.pool.Run(RecalcPiece, TURBO_CHUNKS);
	st.pool.Run(PlaceBand, TURBO_BANDS);
	int lastUsed = 0;
	NUM_PARTS = 0;
	int firstFree = -1, tail = -1;
	for (auto &piece : st.pieces)
	{
		NUM_PARTS += piece.count;
		lastUsed = std::max(lastUsed, piece.lastUsed);
		if (elementRecount)
			for (int t = 0; t < PT_NUM; t++)
				elementCount[t] += piece.elementCount[t];
		if (piece.freeHead < 0)
			continue;
		if (tail < 0)
			firstFree = piece.freeHead;
		else
			parts[tail].life = piece.freeHead;
		tail = piece.freeTail;
	}
	int beyond = (parts_lastActiveIndex >= NPART - 1) ? -1 : parts_lastActiveIndex + 1;
	// the particles whose life ran out, in index order, each put at the front of the free list by kill_part;
	// RecalcFreeParticles starts the list again at the first free slot, dropping those killed before it
	bool started = false;
	for (auto &piece : st.pieces)
		for (int i : piece.kills)
		{
			if (!started && firstFree >= 0 && i > firstFree)
			{
				pfree = firstFree;
				started = true;
			}
			kill_part(i);
		}
	if (tail < 0)
		pfree = beyond;
	else
	{
		if (!started)
			pfree = firstFree;
		parts[tail].life = beyond;
	}
	parts_lastActiveIndex = lastUsed;
	elementRecount = false;
	return true;
}

// body(y0, y1) over bands of [0, rows) on all threads (turbo mode)
void Simulation::TurboRows(int rows, const std::function<void(int, int)> &body)
{
	auto &st = TurboStateFor();
	st.rows = rows;
	st.rowsBody = &body;
	st.pool.Run(RowBand, TURBO_BANDS);
	st.rowsBody = nullptr;
}

bool Simulation::TurboParticleOk(const Particle &p) const
{
	if (!turboElementOk[p.type])
		return false;
	// a ctype that can become the particle's type (molten, frozen, cloned) must not be special, nor turn into
	// something special when it melts or freezes (ICEI and SNOW of a liquid that freezes into LQUD)
	if (p.ctype > 0 && p.ctype < PT_NUM)
	{
		auto &c = elements[p.ctype];
		auto special = [this](int t) { return t > 0 && t < PT_NUM && turboSpecial[t]; };
		if (turboSpecial[p.ctype] || special(c.LowTemperatureTransition) || special(c.HighTemperatureTransition))
			return false;
	}
	return true;
}

void Simulation::TurboUpdateList(const int *list, int count)
{
	UpdateParticleLoop<1>(0, count - 1, list, nullptr);
}

void Simulation::TurboFree()
{
	if (turboState)
	{
		// the threads may still be starting: tell them to stop and leave them the pool
		{
			std::lock_guard<std::mutex> lock(turboState->pool.mutex);
			turboState->pool.quit = true;
		}
		turboState->pool.wake.notify_all();
		turboState = nullptr;
	}
}

bool Simulation::TurboFrame()
{
	double t0 = TurboNow();
	// fields that act on everything at once, and water equalisation, keep the whole frame on one thread
	if (emfield->isEnabled || stressField->enabled || water_equal_test)
		return false;
	// too few particles to be worth sorting into tiles (NUM_PARTS: counted by RecalcFreeParticles this frame)
	if (NUM_PARTS < TURBO_MIN_PARTICLES)
		return false;
	auto &st = TurboStateFor();

	// Which elements are updated in parallel this frame, and which are special: changed only on one thread
	auto &builtin = GetElements();
	static std::vector<char> allowed;
	if (allowed.empty())
	{
		allowed.assign(PT_NUM, 0);
		for (auto *id : turboAllowed)
			for (int t = 1; t < PT_NUM; t++)
				if (builtin[t].Identifier == id)
					allowed[t] = 1;
	}
	for (int t = 0; t < PT_NUM; t++)
	{
		auto &e = elements[t];
		bool luaCallbacks = e.Update != builtin[t].Update || e.Create != builtin[t].Create ||
			e.ChangeType != builtin[t].ChangeType || e.CreateAllowed != builtin[t].CreateAllowed;
		turboSpecial[t] = t != PT_NONE && (e.ChangeType || e.CreateAllowed || (e.Properties & PROP_VEHICLE) || luaCallbacks ||
			t == PT_ION || t == PT_MVSD || t == PT_STKM || t == PT_STKM2 || t == PT_FIGH);
	}
	// liquids search far for somewhere to flow; how far depends on gravity
	int reach = TURBO_REACH_STANDARD;
	bool liquids = true;
	if (grav->IsEnabled() || (gravityMode != 0 && gravityMode != 1 && gravityMode != 3))
		liquids = false;
	else if (gravityMode != 0)
		reach = TURBO_REACH_CUSTOM;
	auto outputOk = [&](int to) {
		return to < 0 || to >= PT_NUM || !turboSpecial[to];
	};
	bool anyOk = false;
	for (int t = 0; t < PT_NUM; t++)
	{
		auto &e = elements[t];
		bool ok = t != PT_NONE && allowed[t] && e.Enabled && !turboSpecial[t] && !(e.Properties & TYPE_ENERGY) &&
			outputOk(e.HighTemperatureTransition) && outputOk(e.LowTemperatureTransition) &&
			outputOk(e.HighPressureTransition) && outputOk(e.LowPressureTransition) &&
			(liquids || !(e.Properties & TYPE_LIQUID));
		turboElementOk[t] = ok;
		anyOk = anyOk || ok;
	}
	if (!anyOk)
		return false;
	if (!liquids)
		reach = TURBO_REACH_NO_LIQUIDS;

	// Tiles: two tiles of the same colour have two tiles between them, more than twice the reach
	int tileSize = ((2 * reach + 1 + 1) / 2 + CELL - 1) / CELL * CELL;
	if (tileSize != st.tileSize || st.tilesX != (XRES + tileSize - 1) / tileSize || st.tilesY != (YRES + tileSize - 1) / tileSize)
	{
		st.tileSize = tileSize;
		st.tilesX = (XRES + tileSize - 1) / tileSize;
		st.tilesY = (YRES + tileSize - 1) / tileSize;
		st.tiles = std::vector<TurboTileSet>(st.tilesX * st.tilesY);
		st.tileOfX.resize(XRES);
		st.tileOfY.resize(YRES);
		for (int x = 0; x < XRES; x++)
			st.tileOfX[x] = x / tileSize;
		for (int y = 0; y < YRES; y++)
			st.tileOfY[y] = (y / tileSize) * st.tilesX;
		for (int ty = 0; ty < st.tilesY; ty++)
			for (int tx = 0; tx < st.tilesX; tx++)
			{
				auto &tile = st.tiles[ty * st.tilesX + tx].tile;
				tile.x0 = tx * tileSize;
				tile.y0 = ty * tileSize;
				tile.x1 = std::min(XRES, tile.x0 + tileSize);
				tile.y1 = std::min(YRES, tile.y0 + tileSize);
			}
	}
	st.reach = reach;
	for (auto &set : st.tiles)
	{
		for (auto &piece : set.pieces)
			piece.clear();
		auto &tile = set.tile;
		tile.deferred.clear();
		tile.freed.clear();
		tile.pool.clear();
		tile.list.clear();
		tile.poolNext = 0;
		tile.parallel = false;
	}

	// Sort the particles into tiles (on all threads, in pieces), noting the cells holding special particles
	st.lastActive = parts_lastActiveIndex;
	st.pool.Run(SortPiece, TURBO_CHUNKS);
	// Detector walls are special too: a particle moving through one sets off a flood fill over the whole wall
	const int cw = XRES / CELL, ch = YRES / CELL;
	st.specialCells.assign(cw * ch, 0);
	for (int cy = 0; cy < ch; cy++)
		for (int cx = 0; cx < cw; cx++)
			if (bmap[cy][cx] == WL_DETECT)
				st.specialCells[cy * cw + cx] = 1;
	for (auto &special : st.chunkSpecial)
		for (int cell : special)
			st.specialCells[cell] = 1;
	st.specialSum.assign((cw + 1) * (ch + 1), 0);
	for (int cy = 0; cy < ch; cy++)
		for (int cx = 0; cx < cw; cx++)
			st.specialSum[(cy + 1) * (cw + 1) + cx + 1] = st.specialCells[cy * cw + cx] +
				st.specialSum[cy * (cw + 1) + cx + 1] + st.specialSum[(cy + 1) * (cw + 1) + cx] - st.specialSum[cy * (cw + 1) + cx];
	auto specialIn = [&](int x0, int y0, int x1, int y1) { // pixels, x1 and y1 exclusive
		int cx0 = std::max(0, x0 / CELL), cy0 = std::max(0, y0 / CELL);
		int cx1 = std::min(cw, (x1 + CELL - 1) / CELL), cy1 = std::min(ch, (y1 + CELL - 1) / CELL);
		if (cx0 >= cx1 || cy0 >= cy1)
			return false;
		return st.specialSum[cy1 * (cw + 1) + cx1] - st.specialSum[cy0 * (cw + 1) + cx1] -
			st.specialSum[cy1 * (cw + 1) + cx0] + st.specialSum[cy0 * (cw + 1) + cx0] > 0;
	};

	int parallelTiles = 0;
	for (auto &set : st.tiles)
	{
		set.count = 0;
		for (auto &piece : set.pieces)
			set.count += piece.size();
		if (!set.count)
			continue;
		auto &tile = set.tile;
		int x0 = tile.x0 - reach, y0 = tile.y0 - reach, x1 = tile.x1 + reach, y1 = tile.y1 + reach;
		// looping edges carry particles to the other side of the world
		if (edgeMode == 2 && (x0 < CELL || y0 < CELL || x1 > XRES - CELL || y1 > YRES - CELL))
			continue;
		if (specialIn(x0, y0, x1, y1))
			continue;
		tile.parallel = true;
		parallelTiles++;
	}
	if (!parallelTiles)
		return false;

	// A new frame for the "updated already" marks
	if (turboDone.size() != size_t(NPART))
		turboDone.assign(NPART, 0);
	if (++turboFrameId == 0)
	{
		std::fill(turboDone.begin(), turboDone.end(), 0);
		turboFrameId = 1;
	}

	// Each parallel tile's free slots and random numbers, in tile order
	unsigned int seedBase = RNG::Ref().gen();
	for (size_t n = 0; n < st.tiles.size(); n++)
	{
		auto &set = st.tiles[n];
		auto &tile = set.tile;
		if (!tile.parallel)
			continue;
		size_t want = 16 + set.count / 32;
		while (tile.pool.size() < want)
		{
			int i = PopFreeParticle();
			if (i < 0)
				break;
			tile.pool.push_back(i);
		}
		tile.rng.seed(seedBase ^ (unsigned int)(n * 2654435761u));
	}
	for (auto &ts : st.threads)
	{
		ts.elementCount.assign(PT_NUM, 0);
		ts.soundAppeared.assign(PT_NUM, 0);
		ts.soundVanished.assign(PT_NUM, 0);
		ts.lastActive = -1;
		ts.specialTouched = 0;
	}

	double t1 = TurboNow();
	// The phases: tiles of one colour at a time, the ones with most particles first
	for (int c = 0; c < TURBO_COLOURS * TURBO_COLOURS; c++)
	{
		st.phase.clear();
		for (int ty = c / TURBO_COLOURS; ty < st.tilesY; ty += TURBO_COLOURS)
			for (int tx = c % TURBO_COLOURS; tx < st.tilesX; tx += TURBO_COLOURS)
			{
				auto &set = st.tiles[ty * st.tilesX + tx];
				if (set.tile.parallel)
					st.phase.push_back(&set);
			}
		std::stable_sort(st.phase.begin(), st.phase.end(), [](TurboTileSet *a, TurboTileSet *b) {
			return a->count > b->count;
		});
		st.pool.Run(UpdateTile, int(st.phase.size()));
	}

	double t2 = TurboNow();
	// Add up what the threads counted, and give back the free slots
	int specialTouched = 0;
	for (auto &ts : st.threads)
	{
		for (int t = 0; t < PT_NUM; t++)
		{
			elementCount[t] += ts.elementCount[t];
			soundAppeared[t] += ts.soundAppeared[t];
			soundVanished[t] += ts.soundVanished[t];
		}
		parts_lastActiveIndex = std::max(parts_lastActiveIndex, ts.lastActive);
		specialTouched += ts.specialTouched;
	}
	int parallelParticles = 0, deferredMoves = 0;
	for (auto it = st.tiles.rbegin(); it != st.tiles.rend(); ++it)
	{
		auto &tile = it->tile;
		for (size_t k = tile.pool.size(); k > tile.poolNext; k--)
		{
			int i = tile.pool[k - 1];
			parts[i].life = pfree;
			pfree = i;
		}
	}
	for (auto &set : st.tiles)
	{
		auto &tile = set.tile;
		for (int i : tile.freed)
		{
			parts[i].life = pfree;
			pfree = i;
		}
		parallelParticles += int(tile.list.size());
	}

	// The moves too long to make in parallel, then everything not updated yet, in the usual order
	for (auto &set : st.tiles)
	{
		auto &tile = set.tile;
		if (!tile.deferred.empty())
		{
			deferredMoves += int(tile.deferred.size());
			UpdateParticleLoop<2>(0, int(tile.deferred.size()) - 1, nullptr, tile.deferred.data());
		}
	}
	turboSerialCount = 0;
	UpdateParticleLoop<3>(0, NPART - 1);

	double t3 = TurboNow();
	turboStats.msPrepare += t1 - t0;
	turboStats.msParallel += t2 - t1;
	turboStats.msAfter += t3 - t2;
	turboStats.frames++;
	turboStats.tiles = int(st.tiles.size());
	turboStats.parallelTiles = parallelTiles;
	turboStats.parallelParticles = parallelParticles;
	turboStats.serialParticles = turboSerialCount;
	turboStats.deferredMoves = deferredMoves;
	turboStats.specialTouched += specialTouched;
	return true;
}
