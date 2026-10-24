#pragma once

#include <array>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

#include "Particle.h"

// Particles waiting inside PRTI/PIPE for a PRTO on the same channel, 8 directions x 80 slots per
// channel. TPT Ultimata's Faraday walls multiply the number of channels (FARADAY_CHANNELS * CHANNELS,
// about 20000), so instead of one fixed array of ~700 MB a channel is allocated the first time it is used.
class PortalBuffers
{
public:
	using Channel = std::array<std::array<Particle, 80>, 8>;
	// channel, direction, slot, particle; only occupied slots are listed
	using Entry = std::tuple<int, short, short, Particle>;

	Channel &operator [](int channel)
	{
		auto &slot = channels[channel];
		if (!slot)
		{
			slot = std::make_unique<Channel>(); // value-initialised: every slot empty
		}
		return *slot;
	}

	void Clear()
	{
		channels.clear();
	}

	std::vector<Entry> Entries() const
	{
		std::vector<Entry> entries;
		for (auto &channel : channels)
		{
			for (short i = 0; i < 8; i++)
			{
				for (short j = 0; j < 80; j++)
				{
					if ((*channel.second)[i][j].type)
					{
						entries.emplace_back(channel.first, i, j, (*channel.second)[i][j]);
					}
				}
			}
		}
		return entries;
	}

	void SetEntries(const std::vector<Entry> &entries)
	{
		Clear();
		for (auto &entry : entries)
		{
			(*this)[std::get<0>(entry)][std::get<1>(entry)][std::get<2>(entry)] = std::get<3>(entry);
		}
	}

private:
	std::map<int, std::unique_ptr<Channel>> channels;
};
