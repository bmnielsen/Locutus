#include <BWAPI.h>
#include "Random.h"

// Simple random number utility class.
// It keeps the state and makes random numbers on demand.

using namespace UAlbertaBot;

Random::Random()
{
	std::random_device seed;
	_rng = std::minstd_rand(seed());
}

// Random number in the range [0,n-1], such as an array index.
int Random::index(int n)
{
	std::uniform_int_distribution<int> uniform_dist(0, n-1);
	return uniform_dist(_rng);
}

// A random bool with the given probability of being true.
// 0 <= probabilility <= 1 is not checked; out of range values are treated as if clipped.
bool Random::flag(double probability)
{
	std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);
	return uniform_dist(_rng) < probability;
}

Random & Random::Instance()
{
	static Random instance;
	return instance;
}
