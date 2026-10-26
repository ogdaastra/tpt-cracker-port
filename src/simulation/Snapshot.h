#pragma once

#include <vector>

#include "Particle.h"
#include "PortalBuffers.h"
#include "Sign.h"
#include "Stickman.h"
#include <json/json.h>

class Snapshot
{
public:
	std::vector<float> AirPressure;
	std::vector<float> AirVelocityX;
	std::vector<float> AirVelocityY;
	std::vector<float> AmbientHeat;

	std::vector<Particle> Particles;

	std::vector<float> GravVelocityX;
	std::vector<float> GravVelocityY;
	std::vector<float> GravValue;
	std::vector<float> GravMap;

	std::vector<unsigned char> BlockMap;
	std::vector<unsigned char> ElecMap;

	std::vector<float> FanVelocityX;
	std::vector<float> FanVelocityY;

	// TPT Ultimata state
	std::vector<short> OneWayDir;
	std::vector<unsigned short> FaradayMap;
	std::vector<char> TimeDilation;
	unsigned int timer = 0;

	std::vector<PortalBuffers::Entry> PortalParticles;
	std::vector<unsigned char> WirelessData;
	std::vector<playerst> stickmen;
	std::vector<sign> signs;

	Json::Value Authors;

	Snapshot() :
		AirPressure(),
		AirVelocityX(),
		AirVelocityY(),
		AmbientHeat(),
		Particles(),
		GravVelocityX(),
		GravVelocityY(),
		GravValue(),
		GravMap(),
		BlockMap(),
		ElecMap(),
		FanVelocityX(),
		FanVelocityY(),
		PortalParticles(),
		WirelessData(),
		stickmen(),
		signs()
	{

	}

	virtual ~Snapshot()
	{

	}

	// Roughly how much memory the snapshot holds (signs and authors left out), for history and rewind caps
	size_t Bytes() const
	{
		auto floats = AirPressure.size() + AirVelocityX.size() + AirVelocityY.size() + AmbientHeat.size() +
			GravVelocityX.size() + GravVelocityY.size() + GravValue.size() + GravMap.size() +
			FanVelocityX.size() + FanVelocityY.size();
		return sizeof(*this) + floats * sizeof(float) + Particles.size() * sizeof(Particle) +
			BlockMap.size() + ElecMap.size() + WirelessData.size() + TimeDilation.size() +
			OneWayDir.size() * sizeof(short) + FaradayMap.size() * sizeof(unsigned short) +
			PortalParticles.size() * sizeof(PortalBuffers::Entry) + stickmen.size() * sizeof(playerst);
	}
};
