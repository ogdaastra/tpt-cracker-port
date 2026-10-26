#pragma once

#include <cstddef>
#include <deque>
#include <memory>

class Simulation;
class Snapshot;
struct SnapshotDelta;

// The last seconds of the simulation, for rewinding: a snapshot every few frames, the newest one whole and the
// older ones as differences from the next newer one (SnapshotDelta), the oldest dropped first when there are
// too many or they take too much memory. Showing one restores it into the simulation; when the simulation
// then carries on from there, everything newer is dropped.
class RewindBuffer
{
public:
	unsigned int intervalFrames = 30; // a snapshot every half second at 60 ticks per second
	size_t maxEntries = 60; // 30 seconds of them
	size_t capBytes = size_t(150) << 20;

	RewindBuffer();
	~RewindBuffer();

	// After every simulated frame (Simulation::afterSimHook): takes a snapshot every intervalFrames frames
	void FrameDone(Simulation &sim);
	void Capture(Simulation &sim);
	// Restores the snapshot stepsBack snapshots before the newest (0 = the newest) into the simulation
	bool Show(Simulation &sim, size_t stepsBack);
	// The simulation goes on from the snapshot being shown: drop everything newer
	void ContinueFromView();
	void Clear();

	size_t Count() const;
	size_t Bytes() const;
	size_t View() const { return view; }

private:
	struct Entry
	{
		std::unique_ptr<SnapshotDelta> delta; // this snapshot = delta->Restore(the next newer one)
		size_t bytes;
	};
	std::deque<Entry> older; // oldest first
	std::unique_ptr<Snapshot> latest;
	size_t latestBytes = 0;
	size_t olderBytes = 0;
	std::unique_ptr<Snapshot> viewSnap; // the snapshot Show restored, view snapshots before the newest
	size_t view = 0;
	unsigned int frames = 0;
	int capturedTick = -1; // Simulation::currentTick at the newest snapshot

	void Trim();
};
