#include "Rewind.h"

#include "Simulation.h"
#include "Snapshot.h"
#include "SnapshotDelta.h"

static size_t SnapshotBytes(const Snapshot &snap)
{
	auto floats = snap.AirPressure.size() + snap.AirVelocityX.size() + snap.AirVelocityY.size() + snap.AmbientHeat.size() +
		snap.GravVelocityX.size() + snap.GravVelocityY.size() + snap.GravValue.size() + snap.GravMap.size() +
		snap.FanVelocityX.size() + snap.FanVelocityY.size();
	return sizeof(snap) + floats * sizeof(float) + snap.Particles.size() * sizeof(Particle) +
		snap.BlockMap.size() + snap.ElecMap.size() + snap.WirelessData.size() + snap.TimeDilation.size() +
		snap.OneWayDir.size() * sizeof(short) + snap.FaradayMap.size() * sizeof(unsigned short) +
		snap.PortalParticles.size() * sizeof(PortalBuffers::Entry) + snap.stickmen.size() * sizeof(playerst);
}

RewindBuffer::RewindBuffer() = default;
RewindBuffer::~RewindBuffer() = default;

size_t RewindBuffer::Count() const
{
	return latest ? older.size() + 1 : 0;
}

size_t RewindBuffer::Bytes() const
{
	return latestBytes + olderBytes;
}

void RewindBuffer::FrameDone(Simulation &sim)
{
	if (view)
	{
		ContinueFromView();
	}
	if (++frames >= intervalFrames)
	{
		frames = 0;
		Capture(sim);
	}
}

void RewindBuffer::Capture(Simulation &sim)
{
	auto snap = sim.CreateSnapshot();
	if (latest)
	{
		auto delta = SnapshotDelta::FromSnapshots(*latest, *snap);
		auto bytes = delta->Bytes();
		older.push_back({ std::move(delta), bytes });
		olderBytes += bytes;
	}
	latest = std::move(snap);
	latestBytes = SnapshotBytes(*latest);
	capturedTick = sim.currentTick;
	Trim();
}

void RewindBuffer::Trim()
{
	while (!older.empty() && (older.size() + 1 > maxEntries || Bytes() > capBytes))
	{
		olderBytes -= older.front().bytes;
		older.pop_front();
	}
	if (view >= Count())
	{
		// the snapshot being shown was dropped; the simulation still holds it, but it can't be found again
		viewSnap.reset();
		view = 0;
	}
}

bool RewindBuffer::Show(Simulation &sim, size_t stepsBack)
{
	// leaving the present: keep it first (the newest snapshot can be up to intervalFrames old), unless the
	// simulation hasn't moved since it was taken
	if (!view && stepsBack && latest && sim.currentTick != capturedTick)
	{
		Capture(sim);
	}
	if (!latest || stepsBack >= Count())
	{
		return false;
	}
	if (stepsBack == 0)
	{
		sim.Restore(*latest);
		viewSnap.reset();
		view = 0;
		return true;
	}
	// walk back from the snapshot shown last if it is newer than the one wanted, else from the newest
	std::unique_ptr<Snapshot> snap;
	size_t at = 0;
	if (viewSnap && view <= stepsBack)
	{
		snap = std::move(viewSnap);
		at = view;
	}
	while (at < stepsBack)
	{
		auto &entry = older[older.size() - 1 - at];
		snap = entry.delta->Restore(snap ? *snap : *latest);
		at++;
	}
	sim.Restore(*snap);
	viewSnap = std::move(snap);
	view = stepsBack;
	return true;
}

void RewindBuffer::ContinueFromView()
{
	if (!view || !viewSnap)
	{
		view = 0;
		return;
	}
	for (size_t i = 0; i < view; i++)
	{
		olderBytes -= older.back().bytes;
		older.pop_back();
	}
	latest = std::move(viewSnap);
	latestBytes = SnapshotBytes(*latest);
	view = 0;
	frames = 0;
}

void RewindBuffer::Clear()
{
	older.clear();
	latest.reset();
	viewSnap.reset();
	latestBytes = olderBytes = 0;
	view = 0;
	frames = 0;
	capturedTick = -1;
}
