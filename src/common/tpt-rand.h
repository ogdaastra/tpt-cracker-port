#ifndef TPT_RAND_
#define TPT_RAND_
#include "Config.h"

#include <stdint.h>
#include "Singleton.h"

class RNG : public Singleton<RNG>
{
private:
	uint64_t s[2];
	uint64_t next();
public:
	unsigned int operator()();
	unsigned int gen();
	int between(int lower, int upper);
	bool chance(int nominator, unsigned int denominator);
	float uniform01();

	RNG();
	void seed(unsigned int sd);
};

extern RNG random_gen;

// Turbo mode (Simulation::TurboFrame) gives each part of the world that is updated in parallel its own
// generator: while it is set on a thread, RNG::Ref() on that thread returns it. Unset (always, outside turbo
// mode), RNG::Ref() is the one shared generator, as before.
extern thread_local RNG *tptThreadRNG;

template<>
inline RNG &Singleton<RNG>::Ref()
{
	static RNG instance;
	if (tptThreadRNG)
		return *tptThreadRNG;
	return instance;
}

#endif /* TPT_RAND_ */
