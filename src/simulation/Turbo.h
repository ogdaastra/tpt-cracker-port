#pragma once
#include "common/tpt-rand.h"
#include <atomic>
#include <cstdint>
#include <vector>

// Turbo mode: the particle update on several threads (Simulation::TurboFrame, Turbo.cpp).
//
// The world is cut into square tiles, coloured like a 2 x 2 checkerboard. In each of four phases the tiles of
// one colour are updated at the same time, on as many threads as there are. Two tiles of the same colour are a
// whole tile apart, which is more than anything a particle does in one update can reach, so they never touch
// the same particle, pixel or air cell. A tile only runs in parallel if every particle within reach of it is of
// an element known to act only nearby (Turbo.cpp, turboAllowed); everything else, and anything that drifted
// out of its tile, is updated afterwards in the usual order on one thread.
//
// Shared state that particles change while they update goes through the helpers below: the free particle list
// (each parallel tile gets its own share of free slots), element counts, the sound counters and the highest
// particle index. Each tile also has its own random number generator. Nothing then depends on which thread
// updates which tile or when, so the result is the same on any number of threads.

// A move longer than this (pixels per frame) is not made in parallel but afterwards (TurboDeferredMove)
constexpr float TURBO_MAX_MOVE = 8.0f;

struct TurboState;

struct TurboDeferredMove
{
	int i, t, x, y, nt, surround_space;
	float pGravX, pGravY;
	unsigned char update_count;
};

struct TurboTile
{
	int x0, y0, x1, y1; // pixels, x1 and y1 exclusive
	bool parallel;
	std::vector<int> bucket; // particles in the tile when the frame started
	std::vector<int> list; // the ones still in it when its phase starts
	std::vector<int> pool; // free particle slots it may use
	size_t poolNext;
	std::vector<int> freed; // slots of particles it removed
	std::vector<TurboDeferredMove> deferred; // moves too long to make in parallel, made afterwards
	RNG rng;
};

struct TurboThreadState
{
	TurboTile *tile = nullptr;
	std::vector<int> elementCount; // changes, added up after the parallel phases
	std::vector<int> soundAppeared, soundVanished;
	int lastActive = -1;
	int specialTouched = 0; // a particle with global side effects was changed in parallel (must stay 0)
};

// Set while this thread updates a tile in parallel
extern thread_local TurboThreadState *turboThread;
